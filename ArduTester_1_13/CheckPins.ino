//#include <avr/io.h>
//#include <stdlib.h>
//#include "Transistortester.h"
//#include "Makefile.h" //J-L


#ifdef FET_Idss
uint16_t expand_FET_quadratic(uint16_t v0, uint16_t v1, uint16_t ii)
// assuming a datapoint of Vgs=v1, Id=ii, tries to calculate Idss (i.e., Id at Vgs=0)
// v0 must be the Vgs at which Id=0
{
   // Quadratic current curve can be calculated by:
   //  v1 = v0 - v1;
   //  return (uint32_t)((uint16_t)((uint32_t)(ii * v0) / v1) * v0) / v1;
   // But this simpler code need 100 bytes more flash than the following code!
   uint8_t drv, dri;	// needed for rounding
   v1 = v0 - v1;
   drv = 0;
   dri = 0;
   for (;;) {
      uint8_t dv, di;
      dv = (v1 + drv) >> 8;
      drv = (v1 + drv) & 0xff;		// remainder of voltage division by 256
//      if (dv == 0) dv = 1;		// prevent infinite loop
      v1 += dv;      // increase v by 0.4 %;  unfortunately the compiler doesn't do this very smartly, insists on creating a 16-bit temporary variable for d
      di = (ii + dri) >> 8;
      dri = (ii + dri) & 0xff;		// remainder of current division by 256
      ii += di;     
      ii += di;      // increase ii by 0.8 %
      if (di > (60000>>8)) {
         return 0;       
      }
         // no Idss measurement if Idss exceeds 40 mA, the ATmega's maximum pin current
         // note that this is actually quite safe, since by the time there's 40 mA running, the Vgs will be 40mA * 20 ohm = 0.8 V, so quite far from 0, so Id will be less than those 40 mA
      if (v1 > v0) return ii;      // V exceeds Vp, so we've reached Vgs=0 without Id exceeding 40 mA, so we can safely do the Idss measurement
   }
}
#endif

//******************************************************************
void CheckPins(uint8_t HighPin, uint8_t LowPin, uint8_t TristatePin)
  {
  /*
  Function for checking the characteristic of a component with the following pin assignment 
  parameters:
  HighPin: Pin, which will be switched to VCC at the beginning
  LowPin: Pin, which will be switch to GND at the beginning
  TristatePin: Pin, which will be undefined at the beginning
  TristatePin will be switched to GND and VCC also .
*/
  struct {
     unsigned int lp_otr;
     unsigned int lp_otrh;
     unsigned int vCEs;
     unsigned int hp1;
     unsigned int hp2;
     unsigned int hp3;
     unsigned int hp4;
     unsigned int rhp;
     unsigned int lp1;
     unsigned int lp2;
     unsigned int tp1;
     unsigned int tp2;
     unsigned int rtp;
  }adc;
  uint8_t LoPinRL;		// mask to switch the LowPin with R_L
#if FLASHEND > 0x1fff
  uint8_t LoPinRH;		// mask to switch the LowPin with R_H
#endif
  uint8_t TriPinRL;		// mask to switch the TristatePin with R_L
  uint8_t TriPinRH;		// mask to switch the TristatePin with R_H
  uint8_t HiPinRL;		// mask to switch the HighPin with RL
  uint8_t HiPinRH;		// mask to switch the HighPin with R_H
  uint8_t HiADCp;		// mask to switch the ADC port High-Pin
  uint8_t LoADCp;		// mask to switch the ADC port Low-Pin
#ifdef SHOW_R_DS
  uint8_t TriADCp;		// mask to switch the ADC port Tristate-Pin
#endif
  uint8_t HiADCm;		// mask to switch the ADC DDR port High-Pin
  uint8_t LoADCm;		// mask to switch the ADC DDR port Low-Pin
  uint8_t PinMSK;
  uint8_t update_pins;		// flag for updating the trans.ebc pins, 1=update
  uint8_t ii;			// temporary variable
  unsigned int tmp16;		// temporary variable
  const uint8_t *addr;
#ifdef COMMON_EMITTER
  unsigned long e_hfe;		// current amplification factor with common emitter
#else
 #warning "hFE measurement without common emitter circuit"
#endif
#ifndef COMMON_COLLECTOR
 #warning "hFE measurement without common collector circuit"
#endif

#ifdef COMMON_COLLECTOR
  unsigned long c_hfe;		// amplification factor for common Collector (Emitter follower)
#endif

  unsigned int volt_dif;
  /*
    switch HighPin directls to VCC 
    switch R_L port for LowPin to GND 
    TristatePin remains switched to input , no action required 
  */
  wdt_reset();
  addr = &PinRLRHADCtab[LowPin-TP_MIN];		// Address of combined RL / RH / ADC pin table
  LoPinRL = pgm_read_byte(addr);		// instruction for LowPin R_L
#if FLASHEND > 0x1fff
 #if (((PIN_RL1 + 1) != PIN_RH1) || ((PIN_RL2 + 1) != PIN_RH2) || ((PIN_RL3 + 1) != PIN_RH3))
  addr += 3;			// address of PinRHtab[LowPin]
  LoPinRH = pgm_read_byte(addr);		// instruction for LowPin R_H
 #else
  LoPinRH = LoPinRL + LoPinRL;				// instruction for LowPin R_H
 #endif
  addr += 3;			// address of PinADCtab[LowPin]
#else		// LoPinRH not used for ATmega8
 #if (((PIN_RL1 + 1) != PIN_RH1) || ((PIN_RL2 + 1) != PIN_RH2) || ((PIN_RL3 + 1) != PIN_RH3))
  addr += 6;			// address of PinADCtab[LowPin]
 #else
  addr += 3;			// address of PinADCtab[LowPin], table PinRHtab is missing
 #endif
#endif
  LoADCp = pgm_read_byte(addr);		// instruction for ADC Low-Pin, including | TXD_VAL

  addr = &PinRLRHADCtab[TristatePin-TP_MIN];
  TriPinRL = pgm_read_byte(addr);	// instruction for TristatePin R_L
#if (((PIN_RL1 + 1) != PIN_RH1) || ((PIN_RL2 + 1) != PIN_RH2) || ((PIN_RL3 + 1) != PIN_RH3))
  addr += 3;			// address of PinRLtab[TristatePin]
  TriPinRH = pgm_read_byte(addr);	// instruction for TristatePin R_H
#else
  TriPinRH = TriPinRL + TriPinRL;			// instruction for TristatePin R_H
#endif
#ifdef SHOW_R_DS
  addr += 3;			// address of PinADCtab[TristatePin]
  TriADCp = pgm_read_byte(addr);	// instruction for ADC Tristate-Pin, including | TXD_VAL
#endif

  addr = &PinRLRHADCtab[HighPin-TP_MIN];
  HiPinRL = pgm_read_byte(addr);		// instruction for HighPin R_L
#if (((PIN_RL1 + 1) != PIN_RH1) || ((PIN_RL2 + 1) != PIN_RH2) || ((PIN_RL3 + 1) != PIN_RH3))
  addr += 3;			// address of PinRHtab[HighPin]
  HiPinRH = pgm_read_byte(addr);		// instruction for HighPin R_H
#else
  HiPinRH = HiPinRL + HiPinRL;				// instruction for HighPin R_H
#endif
  addr += 3;			// address of PinADCtab[HighPin]
  HiADCp = pgm_read_byte(addr);		// instruction for ADC High-Pin, including | TXD_VAL 
  HiADCm = HiADCp | TXD_MSK;
  LoADCm = LoADCp | TXD_MSK;

  //setting of Pins 
#if (PROCESSOR_TYP != 8)
 #define EXACT_OTR
 // with option EXACT_OTR the vCE0 and vCEs is determined with common emitter circuit for
 // PNP and NPN transistors. Without this option set the circuit is common emitter for PNP and
 // common collector for NPN.
#endif
#ifndef EXACT_OTR
  R_PORT = 0;				//resistor-Port outputs to 0
  R_DDR = LoPinRL;			//Low-Pin to output and across R_L to GND
  ADC_DDR = HiADCm;		//High-Pin to output
  ADC_PORT = HiADCp;		//High-Pin fix to Vcc
  //for some MOSFET the gate (TristatePin) must be discharged
  R_DDR = LoPinRL | TriPinRL;
  adc.vCEs = W5msReadADC(LowPin);	// lp1 is the voltage at 680 Ohm with - Gate
  R_DDR = LoPinRL;	
  adc.lp_otr = W5msReadADC(LowPin);	//read voltage of Low-Pin  , without Gate current (-)
  R_DDR = 0;
  wait5ms();				// release all current (clear TRIAC and Thyristor)
  R_PORT = TriPinRL;
  R_DDR = LoPinRL | TriPinRL;		// start current again
  adc.lp2 = W5msReadADC(LowPin);	// lp2 is the voltage at 680 Ohm with + Gate
  R_DDR = LoPinRL;
  R_PORT = 0;
  if (adc.lp2 < adc.vCEs) {
     adc.lp_otr = W5msReadADC(LowPin);	//read voltage of Low-Pin again, without Gate current (+)
     if ((adc.lp2+288) > adc.vCEs) goto checkDiode;	// no significant change
     adc.vCEs = adc.lp2;
  } else {
     if ((adc.vCEs+288) > adc.lp2) goto checkDiode;	// no significant change
  }
#else
  // first try the common emitter circuit for P-Channel type
     uint16_t v_change_n;
     uint16_t v_change_p;
  R_PORT = TriPinRL;			//resistor-Port TriState to 1
  R_DDR = LoPinRL | TriPinRL;		// resistor-Port Low-Pin to 0
  ADC_DDR = HiADCm;		//High-Pin to output
  ADC_PORT = HiADCp;		//High-Pin fix to Vcc
  //for some MOSFET the gate (TristatePin) must be discharged
  adc.vCEs = W5msReadADC(LowPin);	// lp1 is the voltage at 680 Ohm with + Gate
 #ifdef WITH_PUT
  R_PORT = TriPinRL | TriPinRH;
  R_DDR = LoPinRL | TriPinRH;           
  adc.lp_otrh = W5msReadADC(LowPin);	//read voltage of Low-Pin  , with tri-state ping to plus via RH; prevents accidental triggering of PUT
 #endif
  R_DDR = LoPinRL;		// resistor-Port Low-Pin to 0
  adc.lp_otr = W5msReadADC(LowPin);	//read voltage of Low-Pin  , without Gate current (+)
  R_DDR = 0;
  wait10ms();	 			// release all current (clear TRIAC and Thyristor)
  R_PORT = 0;
  R_DDR = LoPinRL | TriPinRL;		// start current again
  adc.lp2 = W5msReadADC(LowPin);	// lp2 is the voltage at 680 Ohm with - Gate
  v_change_p = abs_diff(adc.lp2, adc.vCEs);
     // switch to common emitter for NPN or N-channel FET
     ADC_DDR = LoADCm;		//Low-Pin to output
     ADC_PORT = TXD_VAL;		//Low-Pin fix to GND
     R_PORT = HiPinRL | TriPinRL;
     R_DDR = HiPinRL | TriPinRL;	// resistor-Port High-Pin and TriState-Pin to 1
     adc.hp1 = vcc_diff(W5msReadADC(HighPin));		// voltage at 680 Ohm with + Gate
     R_PORT = 0;			// clear Thyristor
     wait10ms();				// release all current (clear TRIAC and Thyristor)
     R_PORT = HiPinRL;			//resistor-Port High-Pin to +, TriState to 0
     adc.hp2 = vcc_diff(W5msReadADC(HighPin));	// voltage at 680 Ohm with - Gate
     R_DDR = HiPinRL;			// resistor-Port High-Pin to 1, TriState open
     adc.hp3 = vcc_diff(W5msReadADC(HighPin)); // voltage at 680 Ohm with open Gate
     v_change_n = abs_diff(adc.hp1, adc.hp2);
#if DebugOut == 5
  lcd_clear_line2();
  lcd_data('T');
  lcd_data('T');
  lcd_testpin(LowPin);
  lcd_data(',');
  lcd_testpin(HighPin);
  lcd_space();
  lcd_data('-');
  mVOut(adc.vCEs);
  lcd_data('o');
  mVOut(adc.lp_otr);
  lcd_data('+');
  mVOut(adc.lp2);
  lcd_data('o');
  lcd_data('h');
  mVOut(adc.lp_otrh);

  lcd_line3();
  lcd_data('-');
  mVOut(adc.hp2);
  lcd_data('o');
  mVOut(adc.hp3);
  lcd_data('+');
  mVOut(adc.hp1);
#endif 
#if 0
//   if (adc.lp2 < adc.vCEs) {
   if (adc.lp2 < (adc.vCEs+288)) {
     // current is lower with Gate switched to 0
  //   if ((adc.lp2+288) > adc.vCEs) goto checkDiode;	// no significant change
//     if ((adc.lp2+288) > adc.vCEs) goto checkDiode;	// no significant change
     if (abs_diff(adc.hp1, adc.vCEs) < 288) goto checkDiode;    // no significant change
 #ifdef WITH_PUT
     adc.lp_otrh = adc.lp_otr;
 #endif
  } else {
     // if ((adc.vCEs+288) > adc.lp2) goto checkDiode;	// no significant change
     if (abs_diff(adc.lp2, adc.vCEs) < 288) goto checkDiode;    // no significant change
  }
#endif
  if ((v_change_n < 288)  && (v_change_p < 288)) goto checkDiode; // no significant change
//  if ((v_change_n+150) > (v_change_p + adc.hp3))
  if ((adc.hp2 + v_change_p) < (adc.vCEs + v_change_n))
  {
 #if DebugOut == 5
     lcd_data('c');
     lcd_data('N');
 #endif
     adc.vCEs = adc.hp2;	// voltage at + 680 Ohm with - Gate
     adc.lp_otr = adc.hp3;	// voltage at + 680 Ohm with open Gate
  }
#endif

//  ChargePin10ms(TriPinRL,0);		//discharge for N-Kanal
//  adc.lp_otr = W5msReadADC(LowPin);	//read voltage of Low-Pin 
//  if(adc.lp_otr >= 977) {		//no current now? 
//     ChargePin10ms(TriPinRL,1);	 	//else: discharge for P-channel (Gate to VCC)
//     adc.lp_otr = ReadADC(LowPin);	//read voltage of Low-Pin again
//  }

#if DebugOut == 5
  lcd_clear_line2();
#endif
  update_pins = 1;			// if a part is found, always update the pins
  // the tester will never find more that two transistors
  // a TRIAC is marked as two transistors at least (2 or 3)
  // both of NPN transistors (normal and inverse) are found, if ntrans.count == 2
  // both of PNP transistors (normal and inverse) are found, if ptrans.count == 2
  // If Transistor with protection diode is checked, all results are found,
  // if ntrans.count == 1 and ptrans.count == 1
  if ((ntrans.count + ptrans.count) > 1) {
     // all transistors found, no more search is needed
     // but TRIAC can be detected as NPNp with same pins as PNPn
     if (!((ntrans.count == 1) && (ntrans.b == ptrans.b))) {
       goto checkDiode;
     }
  }
//  if(adc.lp_otr > 92) {  //there is some current without TristatePin current 
  // Look for Source/Drain current without gate current.
  // Germanium transistors have also high collector current with higher temperature.
  // But this current is lower, if the base is hold at emitter level (adc.hp1).
  // Additional checking of adc.lp1 will prevent to detect a PNP as P-JFET.
//  if((PartMode <= PART_MODE_P_JFET) && (adc.lp_otr > 455)  && (adc.vCEs > 455))
  
//  if((adc.lp_otr > 455)  && (adc.vCEs > 455))
//  if((adc.vCEs > 115)  && ((adc.vCEs+100) > adc.lp_otr))
  if((adc.vCEs > 115)  && ((adc.vCEs+adc.vCEs+20) > adc.lp_otr))
     {  //there is more than 650uA current without TristatePin current 
     // can be JFET or D-FET
     //Test if N-JFET or if self-conducting N-MOSFET
#ifdef EXACT_OTR
     ADC_DDR = HiADCm;		//High-Pin to output
     ADC_PORT = HiADCp;		//High-Pin fix to Vcc
#endif
     R_DDR = LoPinRL | TriPinRH;	//switch R_H for Tristate-Pin (probably Gate) to GND
     adc.lp1 = W10msReadADC(LowPin);	//measure voltage at the assumed Source 
     adc.tp1 = ReadADC(TristatePin);	// measure Gate voltage
     R_PORT = TriPinRH;			//switch R_H for Tristate-Pin (probably Gate) to VCC
     adc.lp2 = W10msReadADC(LowPin);	//measure voltage at the assumed Source again
#if DebugOut == 5
     lcd_testpin(LowPin);
     lcd_data('f');
     lcd_testpin(HighPin);
     lcd_space();
     mVOut(adc.lp2);
     mVOut(adc.lp1);
     lcd_line3();
     mVOut(adc.vCEs);
     mVOut(adc.lp_otr);
#endif
     //If it is a self-conducting MOSFET or JFET, then must be: adc.lp2 > adc.lp1 
     if(adc.lp2>(adc.lp1+599)) {
        //measure voltage at the  Gate, differ between MOSFET and JFET
        ADC_PORT = TXD_VAL;
        ADC_DDR = LoADCm;	//Low-Pin fix to GND
        R_DDR = TriPinRH | HiPinRL;	//High-Pin to output
        R_PORT = TriPinRH | HiPinRL;	//switch R_L for High-Pin to VCC
        adc.lp2 = W10msReadADC(TristatePin); //read voltage of assumed Gate 
#if DebugOut == 5
        lcd_data('N');
#endif
        if(adc.lp2>3911) {  //MOSFET
           PartFound = PART_FET;	//N-Kanal-MOSFET
           PartMode = PART_MODE_MOS|N_CHANNEL|D_MODE; //Depletion-MOSFET
#if DebugOut == 5
        lcd_data('D');
#endif
        } else {  //JFET (pn-passage between Gate and Source is conducting )
           PartFound = PART_FET;	//N-Kanal-JFET
           PartMode = PART_MODE_JFET|N_CHANNEL|D_MODE;
#if DebugOut == 5
        lcd_data('J');
#endif
        }
#if DebugOut == 5
        lcd_space();
#endif
//      if ((PartReady == 0) || (adc.lp1 > ntrans.uBE)) 
//      there is no way to find out the right Source / Drain
        ntrans.uBE = adc.lp1;
        ntrans.gthvoltage = unsigned_diff(adc.lp1, adc.tp1);	//voltage GS (Source - Gate)
        ntrans.current = (unsigned int)(((unsigned long)adc.lp1 * 10000) / RR680MI); // Id 1uA
#ifdef SHOW_ICE
 // Test for cutoff Voltage, idea from Pieter-Tjerk
        R_DDR = LoPinRH | TriPinRH;
	R_PORT = 0;			// Low + Tristate Pin with 470k to 0V
	ADC_DDR = HiADCm;
	ADC_PORT = HiADCp;		// High Pin to VCC
	ntrans.ice0 = unsigned_diff(W10msReadADC(LowPin), ReadADC(TristatePin)); // Gate-Source Voltage
 #ifdef FET_Idss
  #if DebugOut == 5
    lcd_line4();
    lcd_data('N');
    lcd_data('j');
    lcd_space();
  #endif
    if ((PartMode&0x0f) == PART_MODE_JFET)
        {
           uint16_t i16;
           // extrapolate the quadratic relationship between Id and Vgs, to estimate Idss
           i16 = expand_FET_quadratic(ntrans.ice0,ntrans.gthvoltage,ntrans.current);
  #if DebugOut == 5
           DisplayValue(i16,-6,' ',3);
  #endif
           // i16=0 if estimated Idss would exceed 40 mA, so don't measure then
           if (i16 != 0) {
//               R_PORT = 0; 
               R_DDR = TriPinRH;	// gate to ground via RH
//               ADC_PORT = HiADCp;       // drain to Vcc, source to gnd, both without resistors
               ADC_DDR = HiADCm|LoADCp;
               adc.lp2 = W10msReadADC(LowPin);	//measure voltage at the Source; this is the voltage drop across the pin's ~20 ohm internal resistance!
               ADC_DDR = TXD_MSK;             // disconnect drain and source immediately after measurement, since quite a lot of current may flow
               // this is almost the Idss, since the gate-source voltage is almost 0 (only the voltage drop across that 20 ohm resistance)
               i16 = (unsigned int)(((unsigned long)adc.lp2 * 10000) / pin_rmi); // Idss 1uA
  #if DebugOut == 5
           DisplayValue(i16,-6,' ',3);
  #endif
               i16 = expand_FET_quadratic(ntrans.ice0,adc.lp2,i16);
  #if DebugOut == 5
           DisplayValue(i16,-6,' ',3);
  #endif
           }
           ntrans.uBE = i16;
        }
 #endif
 #ifdef SHOW_R_DS
    if ((PartMode&0x0f) != PART_MODE_JFET)
         {
            // For depletion MOSFET we try to put the Gate to the same level as Source.
            // The source level is higher than 0V because of the Port output resistance and current.
            // When the Drain-Source resistance is low, we can get nearly the same voltage increase
            // at the Gate with current from the 680 Ohm Port (RL).
            ADC_PORT = TXD_VAL;
            ADC_DDR = LoADCm | TriADCp;	//Low-Pin and Tristate-Pin fix to GND
//            R_DDR = TriPinRL | HiPinRL;	// L-Resistor High-Pin and Tristate-Pin to output
            R_DDR =  HiPinRL;	// L-Resistor High-Pin and Tristate-Pin to output
            R_PORT = TriPinRL | HiPinRL;	//switch R_L for High-Pin and Tristate-Pin to VCC
            adc.hp2 = W5msReadADC(HighPin);	//measure the voltage at the Drain  
            adc.rhp = vcc_diff(adc.hp2);	// voltage at the Drain resistor
            adc.lp2 = ReadADC(LowPin);		// voltage at the Source 
            ntrans.uBE = RR680PL * (unsigned long)unsigned_diff(adc.hp2, adc.lp2) / adc.rhp; // DS resistance in 0.1 OHm

        }
 #endif
#endif  /* end SHOW_ICE */
#if DebugOut == 5
        lcd_data('#');
#endif
        ntrans.count++;			// count as two, the inverse is identical
        goto saveNresult;		// save Pin numbers and exit
     }  /* end selfconducting N-channel  mode */

     ADC_PORT = TXD_VAL;		// direct outputs to GND

     //Test, if P-JFET or if self-conducting P-MOSFET
     ADC_DDR = LoADCm;		//switch Low-Pin (assumed Drain) direct to GND,
				//R_H for Tristate-Pin (assumed Gate) is already switched to VCC
     R_DDR = TriPinRH | HiPinRL;	//High-Pin to output
     R_PORT = TriPinRH | HiPinRL;	//High-Pin across R_L to Vcc
     adc.hp1 = W10msReadADC(HighPin);	//measure voltage at assumed Source 
     adc.tp1 = ReadADC(TristatePin);	// measure Gate voltage
     R_PORT = HiPinRL;			//switch R_H for Tristate-Pin (assumed Gate) to GND
     adc.hp2 = W10msReadADC(HighPin);	//read voltage at assumed Source again
     //if it is a self-conducting P_MOSFET or P-JFET , then must be:  adc.hp1 > adc.hp2 
     if(adc.hp1>(adc.hp2+599)) {
        //read voltage at the Gate , to differ between MOSFET and JFET
        ADC_PORT = HiADCp;	//switch High-Pin directly to VCC
        ADC_DDR = HiADCm;	//switch High-Pin to output
        adc.tp2 = W10msReadADC(TristatePin); //read voltage at the assumed Gate 
#if DebugOut == 5
        lcd_data('P');
#endif
        if(adc.tp2<977) { 		//MOSFET
           PartFound = PART_FET;	//P-Kanal-MOSFET
           PartMode = PART_MODE_MOS|P_CHANNEL|D_MODE; //Depletion-MOSFET
#if DebugOut == 5
           lcd_data('D');
#endif
        } else { 			//JFET (pn-passage between Gate and Source is conducting)
           PartFound = PART_FET;	//P-Kanal-JFET
           PartMode = PART_MODE_JFET|P_CHANNEL|D_MODE;
#if DebugOut == 5
           lcd_data('J');
#endif
        }
#if DebugOut == 5
        lcd_space();
#endif
        ptrans.gthvoltage = unsigned_diff(adc.tp1, adc.hp1);	//voltage GS (Gate - Source)
        ptrans.current = (unsigned int)(((unsigned long)vcc_diff(adc.hp1) * 10000) / RR680PL); // Id 1uA
#ifdef SHOW_ICE
//	Test for cutoff Voltage, idea from Pieter-Tjerk
        ADC_PORT = TXD_VAL;		// direct outputs to GND
        ADC_DDR = LoADCm;		//switch Low-Pin (assumed Drain) direct to GND,
        R_DDR = TriPinRH | HiPinRH;	//High-Pin to output
        R_PORT = TriPinRH | HiPinRH;	//High-Pin and Tristate-Pin across R_H to Vcc
        ptrans.ice0 = unsigned_diff(W10msReadADC(TristatePin), ReadADC(HighPin));	//measure voltage at assumed Source 
 #ifdef FET_Idss
  #if DebugOut == 5
        lcd_line4();
        lcd_data('P');
        lcd_data('J');
  #endif
        if ((PartMode&0x0f) == PART_MODE_JFET) 
        {
           uint16_t i16;
           // extrapolate the quadratic relationship between Id and Vgs, to estimate Idss
           i16 = expand_FET_quadratic(ptrans.ice0,ptrans.gthvoltage,ptrans.current);
  #if DebugOut == 5
           DisplayValue(i16,-6,' ',3);
  #endif
           // i16=0 if estimated Idss would exceed 40 mA, so don't measure then
           if (i16 != 0) {
               R_PORT = TriPinRH; 
               R_DDR = TriPinRH;	// gate to VCC via RH
               ADC_PORT = HiADCp;       // drain to GND, source to VCC, both without resistors
               ADC_DDR = HiADCm|LoADCm;
               adc.hp3 = vcc_diff(W10msReadADC(HighPin));	//measure voltage at the Source; this is the voltage drop across the pin's ~20 ohm internal resistance!
               ADC_DDR = TXD_MSK;	// disconnect drain and source immediately after measurement, since quite a lot of current may flow
               // this is almost the Idss, since the gate-source voltage is almost 0 (only the voltage drop across that 20 ohm resistance)
               i16 = (unsigned int)(((unsigned long)adc.hp3 * 10000) / pin_rpl); // Idss 1uA
  #if DebugOut == 5
           DisplayValue(i16,-6,' ',3);
  #endif
               i16 =expand_FET_quadratic(ptrans.ice0,adc.hp3,i16);
  #if DebugOut == 5
           DisplayValue(i16,-6,' ',3);
  #endif
           }
           ptrans.uBE = i16;
        }	/* end if ((PartMode&0x0f) == PART_MODE_JFET) */
 #endif
 #ifdef SHOW_R_DS
    if ((PartMode&0x0f) != PART_MODE_JFET)
         {
            // For depletion MOSFET we try to put the Gate to the same level as Source.
            // The source level is higher than 0V because of the Port output resistance and current.
            // When the Drain-Source resistance is low, we can get nearly the same voltage increase
            // at the Gate with current from the 680 Ohm Port (RL).
            ADC_PORT = HiADCp | TriADCp;	//switch High-Pin and Tristate-Pin to VCC
            ADC_DDR = HiADCm | TriADCp;		//switch High-Pin and Tristate-Pin to output
            R_PORT = 0;			// switch R-Ports to 0
            R_DDR = TriPinRL | HiPinRL;	// L-Resistor High-Pin and Tristate-Pin to output
            adc.hp2 = W5msReadADC(HighPin);	//measure the voltage at the Source  
            adc.lp1 = ReadADC(LowPin);		// voltage at the Drain 
            ptrans.uBE = RR680MI * (unsigned long)unsigned_diff(adc.hp2, adc.lp1) / adc.lp1; // DS resistance in 0.1 OHm
         }	/* end if ((PartMode&0x0f) != PART_MODE_JFET) */
 #endif
#endif
        ptrans.count++;			// count as two, the inverse is identical
        goto savePresult;		// save pin numbers and exit
        }
     // no JFET or D-MOS
     goto checkDiode;	
     } // end component has current without TristatePin signal


  //there is less than 650uA current without TristatePin current 
#ifdef COMMON_COLLECTOR
  // Test circuit with common collector (Emitter follower) PNP
  ADC_PORT = TXD_VAL;
  ADC_DDR = LoADCm;			// Collector direct to GND
#if 0
  R_PORT = HiPinRL;			// switch R_L port for HighPin (Emitter) to VCC
  R_DDR = TriPinRL | HiPinRL;		// Base resistor  R_L to GND
  adc.hp1 = W5msReadADC(HighPin);	// voltage at the Emitter resistor
  adc.rhp = vcc_diff(adc.hp1);	// voltage at the Emitter resistor
  adc.tp1 = ReadADC(TristatePin);	// voltage at the base resistor (RL)

 #if DebugOut == 5
  lcd_line4();
  lcd_data('P');
 #endif
// #############################################################
  if (adc.tp1 < 10) {
     R_DDR = 0;
     wait_about5ms();		// clear TRIAC and Thyristor
     // c_hfe with 1% resolution for optocoupler
     // compute c_hfe with RH base resistor
 #ifdef LONG_HFE
  #if DebugOut == 5
     lcd_data('H');	// PHcc
  #endif
     c_hfe = ((unsigned long)adc.rhp * (unsigned long)(((unsigned long)R_H_VAL * 10000) / 
              (unsigned int)RR680PL)) / (unsigned int)adc.tp1;	
 #else
  #if DebugOut == 5
     lcd_data('h');	// Phcc
  #endif
     c_hfe = ((adc.rhp / ((RR680PL+500)/1000)) * (R_H_VAL/500)) / (adc.tp1/5);
 #endif
  } else {
 #if DebugOut == 5
     lcd_data('L');	// PLcc
 #endif
     // compute c_hfe with RL Base resistor
     c_hfe = (unsigned long)((unsigned long)unsigned_diff(adc.rhp, adc.tp1) * 100) / adc.tp1;
  }
#else
  R_PORT = HiPinRL;			// switch R_L port for HighPin (Emitter) to VCC
  R_DDR =  TriPinRH | HiPinRL;	// Tripin=RH-
  adc.hp1 = W5msReadADC(HighPin);
  adc.rhp = vcc_diff(adc.hp1);	// voltage at the Emitter resistor
  adc.tp1 = ReadADC(TristatePin);	// voltage at base resistor 
 #if DebugOut == 5
  lcd_line4();
  lcd_data('P');
 #endif
  if (adc.rhp > (100+adc.lp_otr)) {

     tmp16 = unsigned_diff(adc.rhp, adc.lp_otr);	// subtract residual current
 #ifdef LONG_HFE
  #if DebugOut == 5
     lcd_data('H');	// PHcc
  #endif
     c_hfe = ((unsigned long)tmp16 * (unsigned long)(((unsigned long)R_H_VAL * 10000) / 
              (unsigned int)RR680PL)) / (unsigned int)adc.tp1;	
 #else
  #if DebugOut == 5
     lcd_data('h');	// Phcc
  #endif
     c_hfe = ((tmp16 / ((RR680PL+500)/1000)) * (R_H_VAL/500)) / (adc.tp1/5);
 #endif
  } else {
     tmp16 = unsigned_diff(adc.rhp, adc.lp_otr);
     R_DDR = TriPinRL | HiPinRL;		// Base resistor  R_L to GND
     adc.hp1 = W5msReadADC(HighPin);	// voltage at the Emitter resistor
     adc.rhp = vcc_diff(adc.hp1);	// voltage at the Emitter resistor
     adc.tp1 = ReadADC(TristatePin);	// voltage at the base resistor (RL)
 #if DebugOut == 5
     lcd_data('L');	// PLcc
 #endif
     // compute c_he with RL Base resistor
     c_hfe = (unsigned long)((unsigned long)unsigned_diff(tmp16, adc.tp1) * 100) / adc.tp1;
  }
#endif
 #if DebugOut == 5
  lcd_data('c');
  lcd_data('c');
  lcd_space();
  lcd_data('e');
  mVOut(adc.rhp);
  lcd_data('o');
  mVOut(adc.lp_otr);
  lcd_data('b');
  mVOut(adc.tp1);
  lcd_data('B');
  lcd_data('=');
  DisplayValue(c_hfe,-2,' ',3);
 #endif
#endif   /* COMMON_COLLECTOR */

  //set Pins again for circuit with common Emitter PNP
  R_DDR = 0;			//all Resistor Ports to Input
  R_PORT = 0;			//switch all resistor ports to GND
  ADC_PORT = HiADCp;		//switch High-Pin to VCC
  ADC_DDR = HiADCm;		//switch High-Pin to output
//  R_DDR = LoPinRL;		//switch R_L port for Low-Pin to output (GND)
  wait_about5ms();
  


#ifndef WITH_PUT
//  if(adc.lp_otr < 1977) 
#else
//  if(adc.lp_otrh < 1977) 
#endif
  {
     //if the component has no connection between  HighPin and LowPin
#if DebugOut == 5
     lcd_line4();
     lcd_testpin(LowPin);
     lcd_data('E');
     lcd_testpin(HighPin);
     lcd_space();
#endif
     //Test to PNP
     R_DDR = LoPinRL | TriPinRL;	//switch R_L port for Tristate-Pin to output (GND), for Test of PNP
     adc.lp1 = W5msReadADC(LowPin);	//measure voltage at LowPin
     if(adc.lp1 > 3422) {
        //component has current => PNP-Transistor or equivalent
//        R_DDR = 0;
//        wait_about5ms();		// clear TRIAC and Thyristor
        //compute current amplification factor in both directions
//#if FLASHEND > 0x1fff
//        R_DDR = LoPinRL | TriPinRL;	//switch R_L port for Tristate-Pin (Base) to output (GND)
//        wait_about5ms();		// load gate capacitor
//        R_DDR = LoPinRL | TriPinRH;	//switch R_H port for Tristate-Pin (Base) to output (GND)
//        adc.lp1 = W5msReadADC(LowPin);	//measure voltage at LowPin (assumed Collector)
//#else
        R_DDR = LoPinRL | TriPinRH;	//switch R_H port for Tristate-Pin (Base) to output (GND)
        adc.lp1 = W10msReadADC(LowPin);	//measure voltage at LowPin (assumed Collector)
//#endif
        adc.tp2 = ReadADC(TristatePin);	//measure voltage at TristatePin (Base) 
        adc.hp2 = ReadADC(HighPin);	//measure voltage at HighPin (assumed Emitter)

        if(adc.tp2 > 2000) {
#ifdef WITH_PUT
           // most likely it's a PNP transistor, but it might be a PUT (Programmable Unijunction Transistor)
           // to check this, make Tri-Pin (assumed base) high again; PNP will stop conducting, PUT won't
           R_PORT = TriPinRH;
           tmp16 = W10msReadADC(LowPin);	//measure voltage at LowPin (assumed Collector)
           if ((tmp16 >= 1024) && (PartFound < PART_TRANSISTOR)) { 
              // compiler could optimize this to an 8-bit compare, but doesn't :-(
 #if DebugOut == 5
              lcd_data('P');
              lcd_data('U');
              lcd_data('T');
              lcd_space();
	      mVOut(tmp16);
 #endif
              // still conducts, can't be a PNP
              PartFound = PART_PUT;
              ptrans.uBE = unsigned_diff(adc.hp2, adc.tp2);	// "Offset Voltage"
              goto savePresult;
           }
#endif
           //PNP-Transistor is found (Base voltage moves with Emitter to VCC)
           PartFound = PART_TRANSISTOR;
           PartMode = PART_MODE_PNP;
           update_pins = 0;		// only update pins, if hFE is higher or Thyristor 
#ifdef COMMON_EMITTER
           //compute current amplification factor for circuit with common Emitter
           //e_hFE = B = Collector current / Base current
 #if DebugOut == 5
           lcd_line4();
           lcd_data('P');
 #endif
           tmp16 = adc.lp1;
           if (tmp16 > adc.lp_otr) {
              tmp16 -= adc.lp_otr;
           }
           // e_hfe with 1% resolution for optocoupler
 #ifdef LONG_HFE
  #if DebugOut == 5
           lcd_data('H');	// PHce
  #endif
           e_hfe = ((unsigned int)tmp16 * (unsigned long)(((unsigned long)R_H_VAL * 10000) / 
              (unsigned int)RR680MI)) / (unsigned int)adc.tp2;	
 #else
  #if DebugOut == 5
           lcd_data('h');	// Phce
  #endif
           e_hfe = ((tmp16 / ((RR680MI+500)/1000)) * (R_H_VAL/500)) / (adc.tp2/5);
 #endif
 #if DebugOut == 5
           lcd_data('c');
           lcd_data('e');
           lcd_space();
           lcd_data('c');
           mVOut(adc.lp1);
           lcd_data('o');
           mVOut(adc.lp_otr);
           lcd_data('b');
           mVOut(adc.tp2);
           lcd_data('B');
           lcd_data('=');
           DisplayValue(e_hfe,-2,' ',3);
 #endif
           // first hFE or e_hfe is greater than last hfe ?
           if ((ptrans.count == 0) || (e_hfe > ptrans.hfe)){
              ptrans.hfe = e_hfe;				// hFE with common emitter
 #if FLASHEND > 0x1fff  /* at least ATmega16 */
              ptrans.current = (unsigned int)(((unsigned long)adc.lp1 * 10000) / RR680MI); // Ic 1uA
 #endif
              ptrans.uBE = unsigned_diff(adc.hp2, adc.tp2);	// Base Emitter Voltage
              update_pins = 1;		// trans.ebc must be updated
           }
#endif
#ifdef COMMON_COLLECTOR
           //current amplification factor for common  Collector (Emitter follower)
           // c_hFE = (Emitter current - Base current) / Base current
 #ifdef COMMON_EMITTER
           // also with COMMON_EMITTER, is c_hfe greater than the last hFE?
           if (c_hfe > ptrans.hfe)  // trans.hfe is allready e_hfe or last c_hFE
 #else
           // without COMMON_EMITTER , c_hFE is first or greater than the old one?
           if ((ptrans.count == 0) || (c_hfe > ptrans.hfe)) 
 #endif
           {
              ptrans.hfe = c_hfe;		// c_hfe is the best
              ptrans.uBE = unsigned_diff(adc.hp1, adc.tp1);	// Base Emitter Voltage common collector
 #if FLASHEND > 0x1fff  /* at least ATmega16 */
              ptrans.current = (unsigned int)(((unsigned long)adc.rhp * 10000) / RR680PL); // Ie 1uA
              ptrans.current += 10000;		// current at emitter!
 #endif
              update_pins = 1;		// trans.ebc must be updated
           }
#endif   /* end COMMON_COLLECTOR */
#if DebugOut == 5
           lcd_data('B');
#endif
#ifdef SHOW_ICE
           if (update_pins != 0) {
	      // update residual collector current without base current
              ptrans.ice0 = (unsigned int)(((unsigned long)adc.lp_otr * 10000) / RR680MI); // ICE0 1uA
              ptrans.ices = (unsigned int)(((unsigned long)adc.vCEs * 10000) / RR680MI); // ICEs 1uA
           }
#endif
           goto savePresult;		// marke P type, save Pins and exit
        }  /* end  if(adc.tp2 > 2000) */
        // is probably a P-E-MOS, check voltage
        if((adc.lp_otr < 97) && (adc.lp1 > 2000)) {
           //is flow voltage low enough in the closed  state?
           //(since D-Mode-FET would be by mistake detected as E-Mode )
           PartFound = PART_FET;		//P-Kanal-MOSFET is found (Basis/Gate moves not to VCC)
           PartMode = PART_MODE_MOS|P_CHANNEL|E_MODE;
           if (adc.hp2 > (adc.lp1+250)) {
 #if DebugOut == 5
           lcd_line4();
           lcd_data('P');
           lcd_data('I');
           lcd_data('G');
           lcd_space();
           mVOut(adc.hp2);
           mVOut(adc.lp1);
 #endif
              //Drain-Source Voltage to high, must be a IGBT
              PartMode = PART_MODE_IGBT|P_CHANNEL|E_MODE;
#ifdef SHOW_R_DS
           } else {
              ptrans.uBE = RR680MI * (unsigned long)unsigned_diff(adc.hp2, adc.lp1) / adc.lp1; // DS resistance in 0.1 OHm
#endif
           }  /* end if (adc.hp2 > (adc.lp1+250)) */
       	   //measure the Gate threshold voltage
           //Switching of Drain is monitored with digital input
           // Low level is specified up to 0.3 * VCC
           // High level is specified above 0.6 * VCC
           PinMSK = LoADCm & 7;
       	   ADMUX = TristatePin | (1<<REFS0);	// switch to TristatePin, Ref. VCC
       	   tmp16 = 1;			// round up ((1*4)/9)
       	   for(ii=0;ii<11;ii++) {
       	      wdt_reset();
       	      ChargePin10ms(TriPinRL,1);
                R_DDR = LoPinRL | TriPinRH;		//switch R_H for Tristate-Pin (Basis) to GND
       	      while (!(ADC_PIN&PinMSK));		// Wait, until the MOSFET switches and Drain moves to VCC
                  			// 1 is detected with more than 2.5V (up to 2.57V) with tests of mega168 and mega328
       	      R_DDR = LoPinRL;
       	      ADCSRA |= (1<<ADSC);		// Start Conversion
       	      while (ADCSRA&(1<<ADSC));		// wait
      	      tmp16 += (1023 - ADCW);	// Add Tristatepin-Voltage
           }
           tmp16 *= 4;		// is equal to 44*ADCW
           ptrans.gthvoltage = tmp16 / 9;		// gives resolution in mV
           ptrans.count++;		// count FET as two for accelerate searching
#if DebugOut == 5
           lcd_data('F');
#endif
           goto savePresult;
        }  /* end P-E-MOS check */
    } /* end component has current => PNP if(adc.lp1 > 3422)  */

#ifdef COMMON_COLLECTOR
#if 0
    // Low-Pin=RL- HighPin=VCC
    R_DDR = LoPinRL | TriPinRL;
    R_PORT = TriPinRL;			// TriPin=RL+  NPN with common Collector
    adc.lp1 = W5msReadADC(LowPin);	// voltage at Emitter resistor
    adc.tp1 = ReadADC(TristatePin);	// voltage at the Base 
    adc.rtp = vcc_diff(adc.tp1);	// voltage at Base resistor
 #if DebugOut == 5
     lcd_line4();
     lcd_data('N');
 #endif
    if (adc.rtp < 10) { 
       R_DDR = 0;
       wait_about5ms();		// clear TRIAC and Thyristor
       R_DDR = LoPinRL | TriPinRH;
       R_PORT = TriPinRH;		// Tripin=RH+
       adc.lp1 = W5msReadADC(LowPin);
       adc.tp1 = ReadADC(TristatePin);	// voltage at the Base 
       adc.rtp = vcc_diff(adc.tp1);	// voltage at Base resistor
       // c_hfe with 1% resolution for optocoupler
       // compute c_hfe with RH Base resistor
       tmp16 = adc.lp1;
       if (tmp16 > adc.lp_otr) {
          tmp16 -= adc.lp_otr;
       }
 #ifdef LONG_HFE
  #if DebugOut == 5
     lcd_data('H');	// NHcc
  #endif
       c_hfe = ((unsigned long)tmp16 * (unsigned long)(((unsigned long)R_H_VAL * 10000) / 
              (unsigned int)RR680MI)) / (unsigned int)adc.rtp;	
 #else
  #if DebugOut == 5
     lcd_data('h');	// Nhcc
  #endif
       c_hfe = ((tmp16 / ((RR680MI+500)/1000)) * (R_H_VAL/500)) / (adc.tp2/5);
 #endif
    } else {
 #if DebugOut == 5
     lcd_data('L');	// NLcc
 #endif
       // compute c_hfe with RL Base resistor
       c_hfe = (unsigned long)((unsigned long)unsigned_diff(adc.lp1, adc.rtp) * 100) / adc.rtp;
    }
#else
 #if DebugOut == 5
     lcd_line4();
     lcd_data('N');
 #endif
    // first try to get hFE with RH
    R_DDR = LoPinRL | TriPinRH;
    R_PORT = TriPinRH;		// Tripin=RH+
    adc.lp1 = W5msReadADC(LowPin);
    adc.tp1 = ReadADC(TristatePin);	// voltage at the Base 
    adc.rtp = vcc_diff(adc.tp1);	// voltage at Base resistor
    if (adc.lp1 > (100+adc.lp_otr)) {
       // c_hfe with 1% resolution for optocoupler
       // compute c_hfe with RH Base resistor
       tmp16 = unsigned_diff(adc.lp1, adc.lp_otr);
 #ifdef LONG_HFE
  #if DebugOut == 5
       lcd_data('H');	// NHcc
  #endif

       c_hfe = ((unsigned long)tmp16 * (unsigned long)(((unsigned long)R_H_VAL * 10000) / 
              (unsigned int)RR680MI)) / (unsigned int)adc.rtp;	
 #else
  #if DebugOut == 5
     lcd_data('h');	// Nhcc
  #endif
       c_hfe = ((tmp16 / ((RR680MI+500)/1000)) * (R_H_VAL/500)) / (adc.tp2/5);
 #endif
    } else {
    // Low-Pin=RL- HighPin=VCC
      R_DDR = LoPinRL | TriPinRL;
      R_PORT = TriPinRL;			// TriPin=RL+  NPN with common Collector
      adc.lp1 = W5msReadADC(LowPin);	// voltage at Emitter resistor
      adc.tp1 = ReadADC(TristatePin);	// voltage at the Base 
      adc.rtp = vcc_diff(adc.tp1);	// voltage at Base resistor
 #if DebugOut == 5
       lcd_data('L');	// NLcc
 #endif
       tmp16 = adc.lp1;
       if (tmp16 > adc.lp_otr) {
          tmp16 -= adc.lp_otr;
       }
       // compute c_hfe with RL Base resistor
       c_hfe = (unsigned long)((unsigned long)unsigned_diff(tmp16, adc.rtp) * 100) / adc.rtp;
    }
#endif
 #if DebugOut == 5
     lcd_data('c');
     lcd_data('c');
     lcd_space();
     lcd_data('e');
     mVOut(adc.lp1);
     lcd_data('o');
     mVOut(adc.lp_otr);
     lcd_data('b');
     mVOut(adc.rtp);
     lcd_data('B');
     lcd_data('=');
     DisplayValue(c_hfe,-2,' ',3);
 #endif
#endif
    //Tristate (can be Base) to VCC, Test if NPN
    ADC_DDR = LoADCm;		//Low-Pin to output 0V
    ADC_PORT = TXD_VAL;			//switch Low-Pin to GND
    R_DDR = TriPinRL | HiPinRL;		//RL port for High-Pin and Tristate-Pin to output
    // vCEs is already measured correctly with common emitter circuit
    R_PORT = TriPinRL | HiPinRL;	//RL port for High-Pin and Tristate-Pin to Vcc
    adc.hp1 = W5msReadADC(HighPin);	//measure voltage at High-Pin  (Collector)
#ifdef WITH_THYRISTOR_GATE_V
    adc.tp2 = ReadADC(TristatePin);	//voltage of gate
    adc.lp2 = ReadADC(LowPin);		//voltage of Cathode
#endif
//    if(adc.hp1 < 1600) 
//    if(adc.hp1 < 4500)    /* limit for opto-coupler with low hFE */
    if(adc.hp1 < 4400)    /* limit for opto-coupler with low hFE */
       {
       //component has current => NPN-Transistor or somthing else

       //Test auf Thyristor:
       //Gate discharge
//      ChargePin10ms(TriPinRL,0);	//Tristate-Pin (Gate) across R_L 10ms to GND
       // TRIAC's can be triggered with gate and A1 (C) swapped. The current remains after triggering 
       // from gate to A2 (A) instead of A1 to A2. I have found that in this state the current will be lower,
       // if the Tristatepin (A1) is switched to GND.
       R_PORT = HiPinRL;
       adc.hp4 = W5msReadADC(HighPin);  //read voltage with switched back base
       R_DDR = HiPinRL;			// base to input
       adc.hp3 = W5msReadADC(HighPin);	//read voltage at High-Pin (probably Anode) again
					//current should still flow, if not,
					// no Thyristor or holding current to low 
		 	
       R_PORT = 0;			//switch R_L for High-Pin (probably Anode) to GND (turn off)
       wait_about5ms();
       R_PORT = HiPinRL;		//switch R_L for High-Pin (probably Anode) again to VCC
       adc.hp2 = W5msReadADC(HighPin);	//measure voltage at the High-Pin (probably Anode) again
#if DebugOut == 5
       lcd_line4();
       lcd_data('y');
       lcd_space();
       mVOut(adc.hp3);
       mVOut(adc.hp2);
       mVOut(adc.hp1);
#endif
       if((adc.hp3 < 1600) && (adc.hp2 > 4400)
           // additional check the voltage hp4 at A with gate hold at GND level
           && ((adc.hp1+150) > adc.hp4)
                                              ) {
          //if the holding current was switched off the thyristor must be switched off too. 
          //if Thyristor was still swiched on, if gate was switched off => Thyristor
          PartFound = PART_THYRISTOR;
#if DebugOut == 5
          lcd_line4();
          lcd_testpin(LowPin);
          lcd_data('Y');
          lcd_testpin(HighPin);
          lcd_space();
#endif
 
          ntrans.count++;		// mark as two N-type transistors
#ifdef WITH_THYRISTOR_GATE_V
          ntrans.uBE = unsigned_diff(adc.tp2, adc.lp2);	// Gate - Cathode Voltage 
          ntrans.gthvoltage = unsigned_diff(adc.hp1, adc.lp2);	// Anode-Cathode Voltage
#endif
          //Test if Triac
          R_DDR = 0;
          R_PORT = 0;
          ADC_PORT = LoADCp;	//Low-Pin fix to VCC
          wait_about5ms();
          R_DDR = HiPinRL;		//switch R_L port HighPin to output (GND)
          if(W5msReadADC(HighPin) > 244) {
#if DebugOut == 15
             lcd_data('H');
             lcd_data('2');
             lcd_space();
#endif
             goto saveNresult;		//measure voltage at the  High-Pin (probably A2); if too high:
                                	//component has current => kein Triac
          }
          R_DDR = HiPinRL | TriPinRL;	//switch R_L port for TristatePin (Gate) to output (GND) => Triac should be triggered 
          if(W5msReadADC(TristatePin) < 977) {
#if DebugOut == 15
             lcd_data('T');
             lcd_data('9');
             lcd_space();
#endif
             goto saveNresult; 		//measure voltage at the Tristate-Pin (probably Gate) ;
                              		// if to low, abort 
          }
          if(ReadADC(HighPin) < 733) {
#if DebugOut == 15
             lcd_data('H');
             lcd_data('7');
             lcd_space();
#endif
             goto saveNresult; 		//component has no current => no Triac => abort
          }
          R_DDR = HiPinRL;		//TristatePin (Gate) to input 
          if(W5msReadADC(HighPin) < 733) {
#if DebugOut == 15
             lcd_data('H');
             lcd_data('3');
             lcd_space();
#endif
             goto saveNresult; 		//component has no current without base current => no Triac => abort
          }
          R_PORT = HiPinRL;		//switch R_L port for HighPin to VCC => switch off holding current 
          wait_about5ms();
          R_PORT = 0;			//switch R_L port for HighPin again to GND; Triac should now switched off
          if(W5msReadADC(HighPin) > 244) {
#if DebugOut == 15
             lcd_data('H');
             lcd_data('4');
             lcd_space();
#endif
             goto saveNresult;		//measure voltage at the High-Pin (probably A2) ;
                                	//if to high, component is not switched off => no Triac, abort
          }
          PartFound = PART_TRIAC;
          goto saveNresult;
        }
      //Test if NPN Transistor or MOSFET
      ADC_DDR = LoADCm;	//Low-Pin to output 0V
 #if FLASHEND > 0x1fff
//      R_DDR = HiPinRL | TriPinRL;	//R_L port of Tristate-Pin (Basis) to output
//      R_PORT = HiPinRL | TriPinRL;	//R_L port of Tristate-Pin (Basis) to VCC
      R_DDR =  TriPinRL;	//R_L port of Tristate-Pin (Basis) to output
      R_PORT = TriPinRL;	//R_L port of Tristate-Pin (Basis) to VCC
      wait_about5ms();			// load gate capacitor
      R_DDR = HiPinRL | TriPinRH;	//R_H port of Tristate-Pin (Basis) to output
      R_PORT = HiPinRL | TriPinRH;	//R_H port of Tristate-Pin (Basis) to VCC
      adc.hp2 = W5msReadADC(HighPin);	//measure the voltage at the collector  
 #else
      R_PORT = HiPinRL | TriPinRH;	//R_H port of Tristate-Pin (Basis) to VCC
      adc.hp2 = W20msReadADC(HighPin);	//measure the voltage at the collector  
 #endif
      adc.rhp = vcc_diff(adc.hp2);	// voltage at the collector resistor
      adc.tp2 = ReadADC(TristatePin);	//measure the voltage at the base 
      adc.rtp = vcc_diff(adc.tp2);	// voltage at the base resistor
      adc.lp2 = ReadADC(LowPin);

//      if((PartFound == PART_TRANSISTOR) || (PartFound == PART_FET)) {
//         PartReady = 1;	//check, if test is already done once
//      }
 #if DebugOut == 5
      lcd_line4();
      lcd_data('r');
      lcd_data('t');
      lcd_data('p');
      lcd_space();
      mVOut(adc.rtp);
 #endif

      if(adc.rtp > 2557) {		// Basis-voltage R_H is low enough
         PartFound = PART_TRANSISTOR;	//NPN-Transistor is found (Base is near GND)
         PartMode = PART_MODE_NPN;
         update_pins = 0;		// only update pins, if better hFE
 #ifdef COMMON_EMITTER
  #if DebugOut == 5
         lcd_line4();
         lcd_data('N');
  #endif
         //compute current amplification factor for common Emitter
         //hFE = B = Collector current / Base current
         tmp16 = unsigned_diff(adc.rhp, adc.lp_otr);	// subtract residual current
         // e_hfe with 1% resolution for optocoupler
  #ifdef LONG_HFE
   #if DebugOut == 5
         lcd_data('H');		// NHce
   #endif
         e_hfe = ((unsigned int)tmp16 * (unsigned long)(((unsigned long)R_H_VAL * 10000) / 
              (unsigned int)RR680PL)) / (unsigned int)adc.rtp;	
  #else
   #if DebugOut == 5
         lcd_data('h');		// NHce
   #endif
         e_hfe = ((tmp16 / ((RR680PL+500)/1000)) * (R_H_VAL/500)) / (adc.rtp/5);
  #endif
 #if DebugOut == 5
         lcd_data('c');
         lcd_data('e');
         lcd_space();
         lcd_data('c');
         mVOut(adc.rhp);
         lcd_data('o');
         mVOut(adc.lp_otr);
         lcd_data('b');
         mVOut(adc.rtp);
         lcd_data('B');
         lcd_data('=');
         DisplayValue(e_hfe,-2,' ',3);
 #endif
         if ((ntrans.count == 0) || (e_hfe > ntrans.hfe)){
            ntrans.hfe = e_hfe;
            ntrans.uBE = unsigned_diff(adc.tp2, adc.lp2);
  #if FLASHEND > 0x1fff  /* at least ATmega16 */
            ntrans.current = (unsigned int)(((unsigned long)adc.rhp * 10000) / RR680PL); // Ic 1uA
  #endif
            update_pins = 1;
         }
 #endif
#ifdef COMMON_COLLECTOR
          //compare current amplification factor for common Collector (Emitter follower)
          // hFE = (Emitterstrom - Basisstrom) / Basisstrom
 #ifdef COMMON_EMITTER
          if (c_hfe >  ntrans.hfe)
 #else
          if ((ntrans.count == 0) || (c_hfe >  ntrans.hfe))
 #endif
          {
            ntrans.hfe = c_hfe;
            ntrans.uBE = unsigned_diff(adc.tp1, adc.lp1);
 #if FLASHEND > 0x1fff  /* at least ATmega16 */
            ntrans.current = (unsigned int)(((unsigned long)adc.lp1 * 10000) / RR680MI); // Ie 1uA
            ntrans.current += 10000;	// mark current at emitter!
 #endif
            update_pins = 1;
          }
#endif
#if DebugOut == 5
         lcd_data('B');
#endif
#ifdef SHOW_ICE
         if (update_pins != 0) {
	    // update residual collector (emitter) current without base current
            ntrans.ice0 = (unsigned int)(((unsigned long)adc.lp_otr * 10000) / RR680MI); // ICE0 0.01mA
            ntrans.ices = (unsigned int)(((unsigned long)adc.vCEs * 10000) / RR680PL); // ICEs 0.01mA
         }
#endif
         goto saveNresult;		// count the found N-Type and exit
      } 
      // Base has low current
      if((adc.lp_otr < 97) && (adc.rhp > 3400)) {
         //if flow voltage in switched off mode low enough?
         //(since D-Mode-FET will be detected in error as E-Mode )
         PartFound = PART_FET;	//N-Kanal-MOSFET is found (Basis/Gate will Not be pulled down)
         PartMode = PART_MODE_MOS|N_CHANNEL|E_MODE;
         if (adc.hp2 > (250+adc.lp2)) {
            // Drain-Source Voltage is too high for N_E_MOS
            PartMode = PART_MODE_IGBT|N_CHANNEL|E_MODE;
#ifdef SHOW_R_DS
         } else {
            ntrans.uBE = RR680PL * (unsigned long)unsigned_diff(adc.hp2, adc.lp2) / adc.rhp; // DS resistance in 0.1 OHm
#endif

         } /* end if (adc.hp2 > (250+adc.lp2)) */
#if DebugOut == 5
         lcd_data('F');
#endif
         //Switching of Drain is monitored with digital input
         // Low level is specified up to 0.3 * VCC
         // High level is specified above 0.6 * VCC
         PinMSK = HiADCm & 7;
         // measure Threshold voltage of Gate
         ADMUX = TristatePin | (1<<REFS0);	// measure TristatePin, Ref. VCC
         tmp16 = 1;			// round up ((1*4)/9)
         for(ii=0;ii<11;ii++) {
            wdt_reset();
            ChargePin10ms(TriPinRL,0);	// discharge Gate 10ms with RL 
            R_DDR = HiPinRL | TriPinRH;	// slowly charge Gate 
            R_PORT = HiPinRL | TriPinRH;
            while ((ADC_PIN&PinMSK));	// Wait, until the MOSFET switch and Drain moved to low 
             		// 0 is detected with input voltage of 2.12V to 2.24V (tested with mega168 & mega328)
            R_DDR = HiPinRL;		// switch off current
            ADCSRA |= (1<<ADSC);		// start ADC conversion
            while (ADCSRA&(1<<ADSC));	// wait until ADC finished
            tmp16 += ADCW;		// add result of ADC
         }
         tmp16 *= 4;	//is equal to 44 * ADCW
         ntrans.gthvoltage = tmp16/ 9;	//scale to mV
         ntrans.count++;		// count FET as two to accelerate  searching
         goto saveNresult;
      }
   } // end component conduct => npn
   ADC_DDR = TXD_MSK;		// switch all ADC-Ports to input
   ADC_PORT = TXD_VAL;		// switch all ADC-Ports to 0 (no Pull up)
   //ready
  }	/* end of component has no connection between HighPin and LowPin */

//##########################################################################################
// Search for diodes
//##########################################################################################
checkDiode:
  R_DDR = 0;			//switch off resistor current
  R_PORT = 0;
  ADC_DDR = TXD_MSK;		// switch ADC ports to input
//  if (adc.lp_otr < 977) 
  if (adc.lp_otr < 455) {
     // current is too low for diode in flow direction, try resistor
#if DebugOut == 5
     lcd_line3();
     lcd_testpin(HighPin);
     lcd_data('D');	// debug
     lcd_testpin(LowPin);
     lcd_space();
     mVOut(adc.lp_otr);
     lcd_data('<');	// debug
#endif
     goto widmes;
  }
  // component has current
  //Test if Diode
  ADC_PORT = TXD_VAL;
  R_DDR = HiPinRH;              // enable resistor from HiPin to ground during the upcoming discharge, so if there's some leakage in the circuit (e.g., via protection diodes) this can't upset the discharge test
  for (ii=0;ii<200;ii++) {
     ADC_DDR = LoADCm | HiADCm; // discharge by short of Low and High side
     wait_about5ms();		// Low and Highpin to GND for discharge
     ADC_DDR = LoADCm;		// switch only Low-Pin fix to GND
     adc.hp1 = ReadADC(HighPin); // read voltage at High-Pin
     if (adc.hp1 < (150/8)) break;
  }
    /*It is possible, that wrong Parts are detected without discharging, because
      the gate of a MOSFET can be charged.
      The additional measurement with the big resistor R_H is made, to differ antiparallel diodes
      from resistors.
      A diode has a voltage, that is nearly independent from the current.
      The voltage of a resistor is proportional to the current.
    */

#if 0
  /* first check with higher current (R_L=680) */
  /* A diode is found better with a parallel mounted capacitor, */
  /* but some capacitors can be detected a a diode. */
  R_DDR = HiPinRL;              //switch R_L port for High-Pin to output (VCC)
  R_PORT = HiPinRL;
  ChargePin10ms(TriPinRL,1);    //discharge of P-Kanal-MOSFET gate
  adc.lp_otr = unsigned_diff(W5msReadADC(HighPin), ReadADC(LowPin));
  R_DDR = HiPinRH;              //switch R_H port for High-Pin output (VCC)
  R_PORT = HiPinRH;
  adc.hp2 = W5msReadADC(HighPin);               // GND--|<--HP--R_H--VCC

  R_DDR = HiPinRL;              //switch R_L port for High-Pin to output (VCC)
  R_PORT = HiPinRL;
  ChargePin10ms(TriPinRL,0);    //discharge for N-Kanal-MOSFET gate
  adc.hp1 = unsigned_diff(W5msReadADC(HighPin), W5msReadADC(LowPin));
  R_DDR = HiPinRH;              //switch R_H port for High-Pin to output (VCC)
  R_PORT = HiPinRH;
  adc.hp3 = W5msReadADC(HighPin);               // GND--|<--HP--R_H--VCC
  if(adc.lp_otr > adc.hp1) {
      adc.hp1 = adc.lp_otr;	//the higher value wins
      adc.hp3 = adc.hp2;
  }
#else
  /* check first with low current (R_H=470k) */
  /* With this method the diode can be better differed from a capacitor, */
  /* but a parallel to a capacitor mounted diode can not be found. */
 #if FLASHEND > 0x1fff
  /* It is difficult to detect the protection diode of D-mode MOSFET . */
  /* We have to generate a negative gate voltage to isolate the diode. */
  /* For P-mode the resistors must reside on the VCC side. */
  /* For N-mode the resistors must be moved to the GND side. */
  R_DDR = HiPinRH;		//switch R_H port for High-Pin output (VCC)
  R_PORT = HiPinRH;
  ChargePin10ms(TriPinRL,1);	//discharge of P-Kanal-MOSFET gate
  adc.hp2 = W5msReadADC(HighPin); 		// GND--|<--HP--R_H--VCC
  // now the resistor is moved to the Low side
  R_DDR = LoPinRH;
  R_PORT = 0;
  ADC_DDR = HiADCm;		// switch High-Pin fix to VCC
  ADC_PORT = HiADCp;
  ChargePin10ms(TriPinRL,0);	//discharge for N-Kanal-MOSFET gate
  adc.hp3 = vcc_diff(W5msReadADC(LowPin)); // GND--R_H--LP--|<--VCC
  /* check with higher current (R_L=680) */
  R_DDR = LoPinRL;
  adc.hp1 = unsigned_diff(W5msReadADC(HighPin), ReadADC(LowPin)); // GND--R_L--LP--|<--VCC
  // test for capacitor behaviour
  adc.lp_otr = unsigned_diff(W5msReadADC(HighPin), ReadADC(LowPin)); // GND--R_L--LP--|<--VCC
#if DebugOut == 5
     lcd_data('C');	// debug
     lcd_data('c');	// debug
     mVOut(adc.hp1);
     mVOut(adc.lp_otr);
#endif
 // if (adc.lp_otr > (adc.hp1+adc.hp1/8))
  if (adc.lp_otr > (adc.hp1+20))
  {
   // voltage increase is too high after twice the wait time, probably capacitor
#if DebugOut == 5
     lcd_data('N');	// debug
     lcd_data('D');	// debug
#endif
     goto clean_ports;
  }
  //  the resistor is moved back to the High side
  ADC_PORT = TXD_VAL;
  ADC_DDR = LoADCm;		// switch only Low-Pin fix to GND
  R_DDR = HiPinRL;		//switch R_L port for High-Pin output (VCC)
  R_PORT = HiPinRL;
  ChargePin10ms(TriPinRL,1);	//discharge for P-Kanal-MOSFET gate
  adc.lp_otr = unsigned_diff(W5msReadADC(HighPin), ReadADC(LowPin)); // GND--|<--HP--R_L--VCC
  if(adc.lp_otr > adc.hp1) {
      adc.hp1 = adc.lp_otr;	//the higher value wins
      adc.hp3 = adc.hp2;
      R_DDR = HiPinRH;		//switch R_H port for High-Pin output (VCC)
      R_PORT = HiPinRH;
      adc.hp2 = W5msReadADC(HighPin); 		// GND--|<--HP--R_H--VCC
  } else {
      R_DDR = LoPinRH;
      R_PORT = 0;
      ADC_DDR = HiADCm;		// switch High-Pin fix to VCC
      ADC_PORT = HiADCp;
      ChargePin10ms(TriPinRL,0);	//discharge for N-Kanal-MOSFET gate
      adc.hp2 = vcc_diff(W5msReadADC(LowPin)); // GND--R_H--LP--|<--VCC
  }
  // move the resistor to the Low side again
 #else
  /* There is not enough space to detect the protection diode for N-D-MOS correctly. */
  R_DDR = HiPinRH;		//switch R_H port for High-Pin output (VCC)
  R_PORT = HiPinRH;
  ChargePin10ms(TriPinRL,1);	//discharge of P-Kanal-MOSFET gate
  adc.hp2 = W5msReadADC(HighPin); 		// GND--|<--HP--R_H--VCC
  ChargePin10ms(TriPinRL,0);	//discharge for N-Kanal-MOSFET gate
  adc.hp3 = W5msReadADC(HighPin);		// GND--|<--HP--R_H--VCC

  /* check with higher current (R_L=680) */
  R_DDR = HiPinRL;		//switch R_L port for High-Pin to output (VCC)
  R_PORT = HiPinRL;
  adc.hp1 = unsigned_diff(W5msReadADC(HighPin), ReadADC(LowPin));
  ChargePin10ms(TriPinRL,1);	//discharge for P-Kanal-MOSFET gate
  adc.lp_otr = unsigned_diff(W5msReadADC(HighPin), ReadADC(LowPin));

  R_DDR = HiPinRH;		//switch R_H port for High-Pin output (VCC)
  R_PORT = HiPinRH;
  if(adc.lp_otr > adc.hp1) {
      adc.hp1 = adc.lp_otr;	//the higher value wins
      adc.hp3 = adc.hp2;
  } else {
      ChargePin10ms(TriPinRL,0);	//discharge for N-Kanal-MOSFET gate
  }
  adc.hp2 = W5msReadADC(HighPin); 		// GND--|<--HP--R_H--VCC
 #endif
#endif
#if DebugOut == 5
  lcd_line3();
  lcd_testpin(HighPin);
  lcd_data('D');	// debug
  lcd_testpin(LowPin);
  lcd_space();
  lcd_data('h');	// debug
  mVOut(adc.hp3);
  lcd_data('L');	// debug
  mVOut(adc.hp1);
  lcd_data('H');	// debug
  mVOut(adc.hp2);
#endif
  volt_dif = adc.hp3/8;
  if (volt_dif > 200) volt_dif = 200;
  if (adc.hp1 < 1000) tmp16 = adc.hp1/100;
  else                tmp16 = adc.hp1/16;

  if((adc.hp1 > 150) && (adc.hp1 < 4640) && (adc.hp2 < adc.hp1) && (adc.hp1 > (adc.hp3+volt_dif)) && (adc.hp3 > tmp16))
     {
     //voltage is above 0,15V and below 4,64V => Ok
     // hp2 >= hp1 is only possible with capacitor, not with a diode, hp2 is measured with 470k
     // (adc.hp3 > adc.hp1/16) was OK for most cases, but not for gate of a TRIAC (where adc.hp1/100 detect the diode)
     // for resistors the expected value is about adc.hp1/670, so adc.hp1/100 should also be OK to differ with resistor
     if(PartFound < PART_DIODE) {
        PartFound = PART_DIODE;	//mark for diode only, if no other component is found
				//since there is a problem with Transistors with a protection diode
#if DebugOut == 5
        lcd_data('D');	// debug  for Diode found
     } else {
        lcd_data('d');  // debug  for Diode, but other part
#endif
     }
     diodes.Anode[NumOfDiodes] = HighPin;
     diodes.Cathode[NumOfDiodes] = LowPin;
     diodes.Voltage[NumOfDiodes] = adc.hp1;	// voltage in Millivolt 
     NumOfDiodes++;
#if DebugOut == 5
     lcd_data(NumOfDiodes+'0');	// debug number of Diodes found
#endif
     goto clean_ports;
  } //end voltage is above 0,15V and below 4,64V 

//##########################################################################################
// Search for resistors
//##########################################################################################
widmes:
  GetResistance(HighPin, LowPin);

//---------------------------------------------------------------------------
// reset the ports and exit
//---------------------------------------------------------------------------
 clean_ports:
#ifdef DebugOut
 #if DebugOut < 10
//  wait_for_key_5s_line2();
 #endif
#endif
  ADC_DDR = TXD_MSK;		// all ADC-Pins Input
  ADC_PORT = TXD_VAL;		// all ADC outputs to Ground, keine Pull up
  R_DDR = 0;			// all resistor-outputs to Input
  R_PORT = 0;			// all resistor-outputs to Ground, no Pull up
  lcd_data('.');		// report advance (suggested by Pieter-Tjerk)
  lcd_refresh();
  return;

//---------------------------------------------------------------------------
// save Pins of P type transistor
//---------------------------------------------------------------------------
savePresult:
#if DebugOut == 5
 lcd_data('P');
#endif
 ptrans.count++;
 if (update_pins != 0) {
    ptrans.b = TristatePin;	// save Pin-constellation
    ptrans.c = LowPin;
    ptrans.e = HighPin;
 }
 goto clean_ports;

//---------------------------------------------------------------------------
// save Pins of N type transistor
//---------------------------------------------------------------------------
saveNresult:
#if DebugOut == 5
 lcd_data('N');
#endif
 ntrans.count++;
 if (update_pins != 0) {
    ntrans.b = TristatePin;	// save Pin-constellation
    ntrans.c = HighPin;
    ntrans.e = LowPin;
 }
 goto clean_ports;

} // end CheckPins()
#ifdef DebugOut
void mVOut(uint16_t vv) {
  DisplayValue(vv,-3,' ',3);
}
#endif

