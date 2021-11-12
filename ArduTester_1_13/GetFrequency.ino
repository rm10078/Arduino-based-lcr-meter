// new code by K.-H. Kübbeler

/*
#include <avr/io.h>
#include <stdlib.h>
#include "Transistortester.h"
#include "Makefile.h" //J-L
#include "config.h" //J-L
*/

//=================================================================
// measure frequency at external pin T0 (PD4@mega328 or PB0@mega644) with Counter 0
//   for mega1280 or mega2560 the T3 (PE6) is used with Counter 3
#if F_CPU > 12000000
 #define FMAX_PERIOD 50020
#else
// #define FMAX_PERIOD 25050
 #define FMAX_PERIOD 33010
#endif
#define FMAX_INPUT 2004000
#define FREQ_DIV 16

#if PROCESSOR_TYP == 1280
 #define PCINTx_vect INT6_vect
#elif PROCESSOR_TYP == 644
 /* Pin change interrupt is PCINT8 (PB0) */
 #define PCI_ENABLE_BIT PCIE1
 #define PCI_CLEAR_BIT PCIF1
 #define PCINTx_vect PCINT1_vect
#else
 /* Pin change interrupt is PCINT20 (PD4) */
 #define PCI_ENABLE_BIT PCIE2
 #define PCI_CLEAR_BIT PCIF2
 #define PCINTx_vect PCINT2_vect
#endif

#if defined(WITH_MENU) && !defined(NO_FREQ_COUNTER)

void GetFrequency(uint8_t range) {
  unsigned char taste;			// set if key is pressed during measurement
 #if PROCESSOR_TYP == 644
  unsigned long freq_count;		// the counted pulses in 1 second
 #endif
  unsigned long long ext_period;
  unsigned long freq_from_per;
  uint8_t ii;
  uint8_t mm;
  /* range has set the lowest bit to use the 16:1 frequency divider permanently. */
  /* The upper bits of range specifies the input selection. */
  /* 0 = external input, 2 = channel 2, 4 = HF Quartz, 6 = LF Quartz */
  
 #if PROCESSOR_TYP == 644
  FDIV_DDR |= (1<<FDIV_PIN);		//switch to output
  if ((range & 0x01) == 0) {
     FDIV_PORT &= ~(1<<FDIV_PIN);	// switch off the 16:1 divider
  } else {
     FDIV_PORT |= (1<<FDIV_PIN);	// use frequency divider for next measurement
  }
  FINP_DDR |= (1<<FINP_P0) | (1<<FINP_P1);	// switch both pins to output

 FINP_PORT &= ~(1<<FINP_P0);		// clear lower bit of input selection
 FINP_PORT &= ~(1<<FINP_P1);		// clear higher bit of input selection
 if (range == 0) {  
    message_key_released(FREQ_str);	// Frequency: in line 1
 } else if (range == 1) { 
    message_key_released(HFREQ_str);	// High Frequency: in line 1
 } else if (range < 4) { /* 2+3 */
    FINP_PORT |= (1<<FINP_P0);		// set lower bit of input selection
    FINP_PORT &= ~(1<<FINP_P1);		// clear higher bit of input selection
 } else if (range < 6) { /* 4+5 */
    FINP_PORT &= ~(1<<FINP_P0);		// clear lower bit of input selection
    FINP_PORT |= (1<<FINP_P1);		// set higher bit of input selection
    message_key_released(H_CRYSTAL_str);	// HF Quarz: in line 1
 } else {  /* 6+7 */
    FINP_PORT |= (1<<FINP_P0);		// set lower bit of input selection
    FINP_PORT |= (1<<FINP_P1);		// set higher bit of input selection
    message_key_released(L_CRYSTAL_str);	// LF Quarz: in line 1
 }

 #else
  message_key_released(FREQ_str);	// Frequency: in line 1
  #ifdef WITH_FREQUENCY_DIVIDER
  uint16_t freq_factor;
  freq_factor = (1<<eeprom_read_byte((uint8_t *)&f_scaler));
  if (freq_factor != 1) {
   #if LCD_LINE_LENGTH > 20
    #define SCALER_POSITION (20 - 5)	// not too wide right
   #else
    #define SCALER_POSITION (LCD_LINE_LENGTH - 5)
   #endif
     lcd_set_cursor(0 * PAGES_PER_LINE, SCALER_POSITION); 
     lcd_data('/');
     u2lcd(freq_factor);	// show the frequency scaler, if not 1
  }
  #endif	/* WITH_FREQUENCY_DIVIDER */
 #endif		/* PROCESSOR_TYP */
  taste = 0;				// reset flag for key pressed
  for (mm=0;mm<240;mm++) {
     // *************************************************************************
     // *********** straight frequency measurement by counting 1 second *********
     // *************************************************************************
     //set up Counter 0
     // Counter 0 is used to count the external signal connected to T0 (PD4 or PB0)
     FREQINP_DDR &= ~(1<<FREQINP_PIN);	// switch frequency pin to input
     wait1ms();				// let capacitor time to load to 2.4V input
#if PROCESSOR_TYP == 1280
     TCCR3A = 0; 			// normal operation, no output
     TCNT3 = 0;				// set counter 3 to zero
     ext_freq.dw = 0;			// set external frequency to zero
     TIFR3 = (1<<TOV3);			// clear OV interrupt of timer 3
     TIMSK3 = (1<<TOIE3);		// enable OV interrupt of timer 3
#else
     TCCR0A = 0; 			// normal operation, no output
     TCNT0 = 0;				// set counter to zero
     ext_freq.dw = 0;			// set external frequency to zero
     TIFR0 = (1<<TOV0);			// clear OV interrupt of timer 0
     TIMSK0 = (1<<TOIE0);		// enable OV interrupt of timer 0
#endif
     // start counter after starting second counter timer 1

     // set up counter 1 to measure one second
     TCCR1A = 0;			// normal operation
#define CNT1_END_VAL ((F_CPU / 256UL) + 1)
#define CNT1_DIVIDER (1<<CS12)
#if (CNT1_END_VAL > 0xffff) && (F_CPU == ((F_CPU / 1024UL) * 1024UL))
 #undef CNT1_END_VAL
 #undef CNT1_DIVIDER
 #define CNT1_END_VAL ((F_CPU / 1024UL) + 1)
 #define CNT1_DIVIDER ((1<<CS12) | (1<<CS10))
#else 
 #if F_CPU != ((F_CPU / 256UL) * 256UL)
  #warning F_CPU can not be divided by 256, measured frequency is wrong!
 #endif
#endif
     OCR1B = (CNT1_END_VAL & 0xffff);	// set to 1 second  (counter 0 is started with 1)
     OCR1A = 1;				// start counter 0 with first count
     TCNT1 = 0;				// set counter to zero
     GTCCR  |= (1<<PSRSYNC);		// reset clock precounter
     TIFR1 = (1<<OCF1B) | (1<<OCF1A);	// clear Output compare match
#if CNT1_END_VAL > 0xffff
     TIMSK1 =  (1<<OCIE1A);		// enable the Compare A match interrupt
#else
     TIMSK1 = (1<<OCIE1B) | (1<<OCIE1A);	// enable the Compare A match and Compare B match interrupt
#endif
     sei();				// set interrupt enable
     TCCR1B = CNT1_DIVIDER;		// divide CPU clock by 256, start counter
     // both counter are running now, wait for counter 1 reach OCR1A
     for (ii=0;ii<50;ii++) {
        wait20ms();			// first count of counter 1 (<32us) has started the counter 0
#if CNT1_END_VAL > 0xffff
        TIMSK1 &=  ~(1<<OCIE1A);	// disable the Compare A match interrupt
        if ((TIFR1 & (1<<TOV1)) != 0) {
          TIFR1 = (1<<OCF1B)|(1<<TOV1);		// reset TOV1 Overflow and Output compare B match Flag
          TIMSK1 = (1<<OCIE1B);		// enable the Compare B match interrupt
        }
#endif
        wdt_reset();
        if (!(RST_PIN_REG & (1<<RST_PIN))) taste = 1;	// user request stop of operation
#if PROCESSOR_TYP == 1280
        if (TCCR3B == 0) break;		// timer 3 is stopped by interrupt
#else
        if (TCCR0B == 0) break;		// timer 0 is stopped by interrupt
#endif
     }
     // one second is counted
#if PROCESSOR_TYP == 1280
     TCCR3B = 0;		// stop timer 3, if not stopped by timer 1 compare interrupt
     ext_freq.w[0] = TCNT3;	// add lower 16 bit to get total counts
#else
     TCCR0B = 0;		// stop timer 0, if not stopped by timer 1 compare interrupt
     ext_freq.b[0] = TCNT0;	// add lower 8 bit to get total counts
#endif
 #if PROCESSOR_TYP == 644
     freq_count = ext_freq.dw;	// save the frequency counter
 #endif
 #if (LCD_LINES > 3)
     lcd_line3();
     lcd_clear_line();
     lcd_line4();
     lcd_clear_line();
     lcd_clear_line2();
 #else
     lcd_clear();		// clear total display
 #endif
     lcd_data('f');
     lcd_equal();		// lcd_data('=');
 #if PROCESSOR_TYP == 644
     if ((FDIV_PORT&(1<<FDIV_PIN)) == 0) {
        Display_Hz(ext_freq.dw, 7);
        lcd_space();		// Frequency divider is not activ
     } else {
        // frequency divider is activ
        Display_Hz(ext_freq.dw*FREQ_DIV, 7);
        lcd_data('/');		// Frequency divider is activ
     }
     lcd_space();
 #else
     // ATmega328 ...
  #ifdef WITH_FREQUENCY_DIVIDER
     uint16_t freq_factor;
     freq_factor = (1<<eeprom_read_byte((uint8_t *)&f_scaler));
     Display_Hz(ext_freq.dw*freq_factor, 7);
  #else
     Display_Hz(ext_freq.dw, 7);
  #endif
 #endif  /* PROCESSOR_TYP 644 or other */
     FREQINP_DDR &= ~(1<<FREQINP_PIN);	// switch frequency pin to input
     if (TCCR1B != 0) {
       // Exact 1000ms period is only with "end of period" from timer1 interrupt.
       // When stopped with the for loop, the time is too long because wait call does not
       // respect CPU time used for interrupts and loop itself.
       // For this case show ? behind the Hz. 
       lcd_data('?');
     }
     TCCR1B = 0;		// stop timer 1
     TIMSK1 = 0;		// disable all timer 1 interrupts
     if ((ext_freq.dw < FMAX_PERIOD) && (ext_freq.dw > 0)) {
     // *************************************************************************
     // ******** Period measurement by counting some periods ******************** 
     // *************************************************************************
        pinchange_max = ((10 * (unsigned long)ext_freq.dw) + MHZ_CPU) / MHZ_CPU;	// about 10000000 clock tics
        pinchange_max += pinchange_max;	// * 2 for up and down change
        FREQINP_DDR &= ~(1<<FREQINP_PIN);	// switch frequency pin to input
        wait1ms();			// let capacitor time to load to 2.4V input
#if PROCESSOR_TYP == 1280
        TCNT3 = 0;			// set counter 3 to zero
        ext_freq.dw = 0;		// reset counter to zero
        TIFR3 = (1<<TOV3);		// clear OV interrupt
        TIMSK3 = (1<<TOIE3);		// enable OV interrupt
        // counter 3 ist started with first pin change interrupt
        pinchange_count = 0;
	EICRB = (0<<ISC61) | (1<<ISC60); // set int6 pin change
        EIFR  |= (1<<INTF6);		// clear interrupt 6 flag
        PCMSK_FREQ |= (1<<PCINT_FREQ); // enable int6
#else
        // other than PROCESSOR_TYP 1280
        TCNT0 = 0;			// set counter 0 to zero
        ext_freq.dw = 0;		// reset counter to zero
        TIFR0 = (1<<TOV0);		// clear OV interrupt
        TIMSK0 = (1<<TOIE0);		// enable OV interrupt
        // counter 0 ist started with first pin change interrupt
        pinchange_count = 0;
        PCIFR  = (1<<PCI_CLEAR_BIT);		// clear Pin Change Status
        PCICR  |= (1<<PCI_ENABLE_BIT);		// enable pin change interrupt
#endif  /* PROCESSOR_1280 or other */
        sei();
        PCMSK_FREQ |= (1<<PCINT_FREQ);	// monitor PD4 PCINT20 or PB0 PCINT8 pin change
        for (ii=0;ii<250;ii++) {
           wait20ms();
           wdt_reset();
           if (!(RST_PIN_REG & (1<<RST_PIN))) taste = 1;	// user request stop of operation
           if ((PCMSK_FREQ & (1<<PCINT_FREQ)) == 0) break;		// monitoring is disabled by interrupt
        } /* end for ii */
#if PROCESSOR_TYP == 1280
        TCCR3B = 0;		// stop counter 3
        PCMSK_FREQ &= ~(1<<PCINT_FREQ); // disable int6
        ext_freq.w[0] = TCNT3;		// add lower 16 bit to get total counts
#else
        TCCR0B = 0;		// stop counter 0
        PCMSK_FREQ &= ~(1<<PCINT_FREQ);		// stop monitor PD4 PCINT20 or PB0 PCINT8 pin change
        PCICR &= ~(1<<PCI_ENABLE_BIT);	// disable the interrupt
        ext_freq.b[0] = TCNT0;		// add lower 8 bit to get total counts
#endif
//        lcd_clear_line2();
//        wait50ms();		// let LCD flicker to 
//	---------------------------------------
//	Show the result from period measurement
//	---------------------------------------
 #if (LCD_LINES > 3)
        lcd_line3();		// use line3 to report the period with 4-line LCD
 #else
        lcd_line2();		// report period on line 2 of 2-line LCD
 #endif
        lcd_data('T');
        lcd_equal();		// lcd_data('=');
        ext_period = ((unsigned long long)ext_freq.dw * (200000/MHZ_CPU)) / pinchange_max;
 #if PROCESSOR_TYP == 644
        if ((FDIV_PORT&(1<<FDIV_PIN)) != 0) {
           // frequency divider is activ, period is measured too long
           ext_period = ext_period / FREQ_DIV;
        }
 #endif
 #if (PROCESSOR_TYP != 644) && defined(WITH_FREQUENCY_DIVIDER)
        ext_period /= freq_factor;
 #endif
        if (pinchange_max > 127) {
           DisplayValue(ext_period,-11,'s',7);	// show period converted to 0.01ns units
        } else {
           //prevent overflow of 32-Bit
           DisplayValue((unsigned long)(ext_period/100),-9,'s',7);	// show period converted to 1ns units
        }
//	---------------------------------------
        if (ii == 250) {
           lcd_data('?');		// wait loop has regular finished
					// probably the input frequency has changed
        } else {
           if (ext_period > 46566) {
//	      ----------------------------------------------------
//	      Show the frequency recalculated from measured period
//	      ----------------------------------------------------
 #if (LCD_LINES > 3)
              lcd_line4();		// use line 4 of 4-line LCD to report the computed frequency
 #else
              lcd_line1();		// overwrite line 1 of 2-line LCD to report the computed frequency
 #endif
              lcd_data('f');
              lcd_equal();		// lcd_data('=');
              if (ext_period > 1000000000) {
                 // frequency in 0.000001Hz (1e11*1e6)/(0.01ns count)
                 freq_from_per = (unsigned long long)(100000000000000000) / ext_period;
                 DisplayValue(freq_from_per,-6,'H',7);  // display with  0.000001 Hz resolution
              } else {
                 // prevent unsigned long overflow, scale to 0.0001 Hz
                 // frequency in 0.001Hz (1e11*1e3)/(0.01ns count)
                 freq_from_per = (unsigned long long)(100000000000000) / ext_period;
                 DisplayValue(freq_from_per,-3,'H',7);  // display with  0.001 Hz resolution
              }
              lcd_data('z');
              FREQINP_DDR &= ~(1<<FREQINP_PIN);	// switch frequency pin to input
//	      ----------------------------------------------------
           }
        }
     }  /* end if 1 < ext_freq < FMAX_PERIOD */
 #if PROCESSOR_TYP == 644
     if ((FDIV_PORT & (1<<FDIV_PIN)) == 0) {
        // frequency divider is not activ
        if ( ((freq_count >= FMAX_PERIOD) && (freq_count < ((unsigned long)FMAX_PERIOD*FREQ_DIV))) ||
            (freq_count > FMAX_INPUT) ){
           FDIV_PORT |= (1<<FDIV_PIN);			// use frequency divider for next measurement
        }
     } else {
        // frequency divider is activ
        if ((freq_count < (FMAX_PERIOD/FREQ_DIV)) && ((range & 0x01) == 0)) {
           FDIV_PORT &= ~(1<<FDIV_PIN);			// switch off the 16:1 divider
        }
     }
 #endif
//     taste += wait_for_key_ms(SHORT_WAIT_TIME/2);
     TIMSK0 = 0;		// disable all timer 0 interrupts
     taste += wait_for_key_ms(2000);
 #ifdef WITH_ROTARY_SWITCH
     if ((taste != 0) || (rotary.incre > 2)) break;
 #else
     if (taste != 0) break;
 #endif
  }  /* end for mm  */
 
  return;
 } // end GetFrequency()

#if PROCESSOR_TYP == 1280
/* ************************************************************ */
/* timer 3 Overflow interrupt                                   */
/* timer 3 count up to 0xffff, then OV occur. update upper part */
/* to build the total counts within one second or               */
/* to build the total counts within the specified pin changes.  */
/* ************************************************************ */
ISR(TIMER3_OVF_vect, ISR_BLOCK) {
  sei();		// set interrupt enable
  ext_freq.w[1] += 1;	// add 65536 clock tics to the total time
}
#else
/* ************************************************************ */
/* timer 0 Overflow interrupt                                   */
/* timer 0 count up to 0xff, then OV occur. update upper part   */
/* to build the total counts within one second or               */
/* to build the total counts within the specified pin changes.  */
/* ************************************************************ */
ISR(TIMER0_OVF_vect, ISR_BLOCK) {
  sei();		// set interrupt enable
  ext_freq.dw += 256;	// add 256 clock tics to the total time
}
#endif

#endif // WITH_MENU

#if defined(WITH_MENU) || ( defined(SamplingADC) && !defined(SamplingADC_CNT))
/* ************************************************************ */
/* Timer 1 Compare A interrupts with count 1 to start counter 0 */
/* This is defined as start of the measurement second.          */
/* ************************************************************ */
// this handler is also used by SamplingADC.S, where simply any dummy interrupt handler is usable; but if timing of this handler changes, need to adapt SamplingADC.S as well
ISR(TIMER1_COMPA_vect, ISR_BLOCK) {
#if PROCESSOR_TYP == 1280
  TCCR3B = (1<<CS32) | (1<<CS31) | (0<<CS30);	// start the counter 3 with external input T3
#else
  TCCR0B = (1<<CS02) | (1<<CS01) | (0<<CS00);	// now start the counter 0 with external input T0
#endif
}
#endif  // WITH_MENU or SamplingADC

#ifdef WITH_MENU
/* ************************************************************ */
/* Timer 1 Compare B interrupt after 1 second to stop counter 0 */
/* ************************************************************ */
ISR(TIMER1_COMPB_vect, ISR_BLOCK) {
  // The TIMER1_COMPB_vect uses one push and one ldi more than this interrupt.
  // Therefore we stop timer 1 first and ajust to same time with wdt_reset();
  wdt_reset();			// for adjusting to same time as TIMER1_COMPB_vect
  TCCR1B = 0;			// stop counter 1
#if PROCESSOR_TYP == 1280
  TCCR3B = 0;			// stop counter 3
#else
  TCCR0B = 0;			// stop counter 0
#endif
}

/* ************************************************************ */
/* Pin change Interrupt                                         */
/* this interrupt is used to measure the period of input signal */
/* the counter 0 is started with pin change interrupt and       */
/* stopped after pin_changed_max changes                        */
/* The pin_changed_max value should be equal to measure         */
/* full periods  (no half period)				*/
/* ************************************************************ */


ISR(PCINTx_vect, ISR_BLOCK)
{
  if (pinchange_count == 0) {
#if PROCESSOR_TYP == 1280
     TCCR3B = (1<<CS30);	// start the counter 3 with full CPU clock
#else
     TCCR0B = (1<<CS00);	// start the counter 0 with full CPU clock
#endif
  }
  if (pinchange_count >= pinchange_max) {
#if PROCESSOR_TYP == 1280
     // stop the counter 3, when maximum value has reached.
     TCCR3B = 0;		// stop counter 3
     PCMSK_FREQ &= ~(1<<PCINT_FREQ);	// disable int6
#else
     // stop the counter 0, when maximum value has reached.
     TCCR0B = 0;		// stop counter 0
     PCMSK_FREQ &= ~(1<<PCINT_FREQ);	// disable monitoring of PD4 PCINT20 or PB0 PCINT8 pin change
#endif
//     PCICR &= ~(1<<PCI_ENABLE_BIT);	// disable the interrupt
  }
  pinchange_count++;
}
#endif
