
// original Dec 2015 - Jan 2016, pa3fwm@amsat.org

//#include "Makefile.h" //J-L

#ifdef WITH_XTAL

#if (MHZ_CPU != 16) && (MHZ_CPU != 20)
   #error "CPU clocks other than 16 MHz or 20 MHz not yet supported for crystal measurement"
#endif

#ifdef SamplingADC_CNT
   #error WITH_XTAL not compatible with SamplingADC_CNT
#endif

#ifndef SamplingADC
   #error WITH_XTAL needs SamplingADC
#endif


#include <avr/io.h>
#include <stdlib.h>
#include "Transistortester.h"

typedef uint8_t byte;



//#define DEB_SAM 10



int16_t sumi;
int16_t sumq;

static void minifourier(int8_t uu[], int16_t freq, byte nuu, byte phase0)
{
   // computes approximation of one term of Fourier transform of uu[0...nuu-1] at frequency freq with initial phase phase0
   // result is returned in global variables sumi and sumq
   // freq is 16-bit fractional frequency, i.e., phase increments by freq/65536*2*pi per element of uu[]
   // phase0 is 8-bit fractional
   byte i;                     // index in uu[]
   int16_t phi16 = phase0<<8;    // 16-bit phase of the "oscillator"
   int16_t sumi1,sumq1;
   sumi1 = sumq1 = 0;
   for (i=0;i<nuu;i++) 
   {
      int8_t u = uu[i];
      byte phi = phi16>>8;
 /*
      int8_t v = phi;
      if (v<0) v = -v;
      v -= 64;
      sumi1 += (u*v)>>6;
      v = phi - 64;
      if (v < 0) v = -v;
      v -= 64;
      sumq1 += (u*v) >> 6;
 */
 // saves some 32 bytes of flash, at cost of readability:
      int8_t v;
      int8_t ph;
      int16_t a;
      ph = phi;
again:
      v = ph;
      if (v<0) v = -v;
      v -= 64;
      a = (u*v)>>6;
      if (ph == (int8_t)phi) {
         sumi1 += a;
         ph -= 64;
         goto again;
      }
      sumq1 += a;
      phi16 += freq;
#if ((DEB_UART & 0x02) != 0)
      if (what == 1) {      myuart_putc('f'); myuart_putc(' '); uart_int(i); uart_int(1000+sumi); uart_int(1000+sumq); uart_int(1000+u); uart_int(1000+sumi-prevsumi); uart_int(uu[i]); uart_newline(); wdt_reset();}
#endif
   }
   sumi = sumi1; sumq = sumq1;
}



static byte findphase(int8_t uu[], int16_t freq, byte nuu)
{
   // find phase at which minifourier().sumi is maximal for given frequency
   // (poor man's arctangent)
   byte phase = 0;
   byte bit = 0x40;
   bit = 0x80;
   // binary search for the upward zero-crossing of sumq, to find maximum of sumi; works because this signal is pretty "well behaved"
   while (bit) {
      minifourier(uu,freq,nuu,phase+bit); 
#if ((DEB_UART & 0x04) != 0)
      {      myuart_putc('f'); myuart_putc(' '); uart_int(phase+bit); uart_int(bit); uart_int(1000+sumi); uart_int(1000+sumq); uart_newline(); wdt_reset();}
#endif
      if (sumq < 0) phase += bit;
      bit >>= 1;
   }
   return phase;
}


static void show_progress(void)
{
   lcd_data('.');
   lcd_refresh();
}

static uint8_t xtal_cancel;   // flag set to 1 to inform caller that user wants to cancel measurement; ugly to do this in global variable, but saves flash compared to properly doing it via the return values


static uint16_t findfreqmax(int8_t uu[], byte nuu, byte minfreq)
{
   // find frequency for which minifourier() is largest

   // first a coarse search
   show_progress();
   uint8_t i;
   byte imax = 0;
   int16_t maxi = 0;
   for (i = minfreq;i <= 129;i+=1)
   {
#if 1
      // simpler code, saves flash, would be a bit slower, which we compensate by considering only half the data in this coarse search
      findphase(uu,i<<8,nuu/2);
      if (sumi > maxi) { imax = i; maxi = sumi; }
#else
      // somewhat faster code because of simpler calculations, but takes some 100 bytes more flash
      minifourier(uu,i<<8,nuu,0);
      if (sumi < 0) sumi = -sumi;
      if (sumq < 0) sumq = -sumq;
      sumiq = sumi + sumq;
      if (sumiq < mini) mini = sumiq;
      sumiq -= mini;
 #if ((DEB_UART & 0x08) != 0)
      if(what == 2) { myuart_putc('r'); myuart_putc(' '); uart_int(i); uart_int(sumiq); uart_int(sumiq+sumiq1+sumiq2); uart_newline(); wdt_reset(); }
 #endif
      wdt_reset();
      if (!(RST_PIN_REG & (1<<RST_PIN))) { xtal_cancel=1; return 0; }	  
      if ((sumiq + sumiq1 + sumiq2) > maxi) { maxi = sumiq + sumiq1 + sumiq2; imax = i - 1; }
      sumiq2=sumiq1;
      sumiq1=sumiq;
#endif
#if ((DEB_UART & 0x08) != 0)
      {            myuart_putc('r'); myuart_putc(' '); uart_int(i); uart_int(10000+sumq); uart_int(10000+sumi); uart_int(imax); uart_newline(); wdt_reset(); }
#endif
   }
   show_progress();

   // then a more refined search
   uint16_t imaxx;
   imaxx = imax;
   uint16_t imax2 = 0;
   int16_t ii;
   maxi = 0;
   for (ii = (imaxx-2)<<8;ii <= (imaxx+2)<<8;ii+=16)  
   {
      findphase(uu,ii,nuu);
      // find maximum
      if (sumi > maxi) { imax2 = ii; maxi = sumi; }
#if ((DEB_UART & 0x08) != 0)
//      if (what==2)  {            myuart_putc('w'); myuart_putc(' '); uart_int(ii); uart_int(10000+sumq); uart_int(10000+sumi); uart_newline(); wdt_reset(); }
      {            myuart_putc('w'); myuart_putc(' '); uart_int(ii); uart_int(10000+sumq); uart_int(10000+sumi); uart_newline(); wdt_reset(); }
#endif
      wdt_reset();
      if (!(RST_PIN_REG & (1<<RST_PIN))) { xtal_cancel = 1; return 0; }
   }
   show_progress();
   // now try to find maximum with more precision, by searching for equally high points on both skirts
   // binary search; may not be the best idea, some smoothing/integration (like in peaksearch()) might be better
   {
      i=128;
      do {
         imax2 += i;
         findphase(uu,imax2+0,nuu);
         int16_t a = sumi;
         findphase(uu,imax2-256,nuu);
         if (a < sumi) imax2 -= i;
         i >>= 1;
      } while (i);
      imax2 -= 128;
      wdt_reset();
   }
   show_progress();
   return imax2;
}


static uint16_t sumabs8(int8_t uu[], byte nuu)
// computes sum of absolute values of uu[]
// this is a measure for "how much" oscillation there is
{
   uint16_t sumd2 = 0;
   byte i;
   for (i=0;i<nuu;i++)
   {
      int8_t u = uu[i];
      if (u<0) u = -u;
      sumd2 += u;
   }
   return sumd2;
}



// variables which store information between sampling_test_xtal() and sampling_measure_xtal(), about how the DUT can be excited
static byte wht;       // bits to be passed as 'what' parameter to samplingADC()
#define isXtal (wht&samplingADC_freq)
static uint16_t dmax = 0;    // optimal interpulse distance for samplingADC() call

void sampling_test_xtal()
// check whether there is a ceramic resonator or crystal connected
// do not yet do precise measurement since that takes quite a bit of time
{
   byte LowPin = TP1;
   byte HighPin = TP3;
   byte HiPinR_L, LoADC;
   HiPinR_L = pinmaskRL(HighPin);
   byte HiPinR_H = pinmaskRH(HighPin);
   LoADC = pinmaskADC(LowPin);
   byte what;

   int8_t ub[255];

   ADC_PORT = TXD_VAL;
   ADC_DDR = LoADC;			// switch Low-Pin to output (GND)
   wait100us();

   ADMUX=HighPin|ADref1V1;   // use built-in reference, about 1.1 V
   wait_aref_stabilize();                               

   what = samplingADC_mux;    // we let the ADC MUX switch back and forth between both sides of the crystal, thus ensuring that the "high" end of the crystal will have roughly the same DC potential as the low end
                             // note that the use of TP1 (MUX=0) for low side is hardcoded!
   what |= samplingADC_8bit|samplingADC_hpf;

   // run a first measurement using a single impulse
   // this allows us to detect ceramic resonators
   samplingADC(what|(1<<smplADC_span), ub, 255, HiPinR_L, HiPinR_H, 0, HiPinR_L);
#if ((DEB_UART & 0x10) != 0)
   { byte i;for (i=0;i<255;i++) { myuart_putc('a'); myuart_putc(' '); uart_int(uu[i]); uart_newline(); wdt_reset(); } }
   { byte i;for (i=0;i<255;i++) { myuart_putc('a'); myuart_putc(' '); uart_int(1000+ub[i]); uart_newline(); wdt_reset(); } }
#endif
   uint16_t sumd = sumabs8(ub+1,254);
//   { myuart_putc('d'); myuart_putc(' '); uart_int(sumd); uart_newline(); wdt_reset(); }
//   lcd_line4();
//   DisplayValue16(sumd,0,' ',5);
//   wait_about5s();
   if (sumd >= 3072) {
      PartFound = PART_CERAMICRESONATOR;
      wht = what;
      return;
   }

   // if more than 100 pF capacitance, than it's very unlikely to be a crystal: don't waste time testing it, don't take risk of incorrectly concluding it is one
   if (cap.cval_max > 100) return;


   // next try a bunch of pulses, at different intervals
   // first try all possible intervals in somewhat coarse steps
   // next repeat, but with finer steps in a limited range around the best one found in the first iteration
   // both times, we check how much oscillation there is during 10 consecutive samples (not more, to save time)
   // the pulse interval having most oscillation is of course the one we want
   // in both scans we also keep track of the "average" amount of oscillation over all pulse intervals
   // in the first scan, this is a reasonable measure of the "background" level, i.e., on freqs where the crystal is not excited
   // the second scan only covers frequencies very near the peak, so there the average is a robust (outlier-insensitive) measure for how much response we get from the crystal
   // we declare a crystal detect if that second average is at least twice the first
   uint16_t maxsumd = 0; // highest amount of oscillation seen so far
   uint16_t d0,d1,ds;   // minimum, maximum interval, and stepsize
   uint16_t avg;        // sum of amount of oscillation, to later computer average from, for current scan
   uint8_t avg0=avg0;   // same average but from first scan, times 2, since it'll be used as threshold; self-initialization suppresses compiler warning

   d0 = 0;                // parameters for the first scan: cover the entire useful range, in steps of 4
   d1 = 8*256+64;
   ds = 4;
   what |= samplingADC_freq;
   what |= samplingADC_many;
   for (;;) { 
      avg = 0;
      uint16_t d;
      for (d=d0;d<d1;d+=ds) {
         // we acquire data for a short time (only 10 samples), because we need to try so many different intervals, otherwise it would take too long
         samplingADC_freqgen((1<<smplADC_span)|what, ub, 10, HiPinR_L, HiPinR_H, 0, HiPinR_L, d);
#if ((DEB_UART & 0x10) != 0)
   if ((d&0x3f) == 0) { byte i;for (i=0;i<10;i++) { myuart_putc('B'); myuart_putc(' '); uart_int(uu[i]); uart_int(d); uart_int(i); uart_newline(); wdt_reset(); } }
   if (d == 472){ byte i;for (i=0;i<10;i++) { myuart_putc('a'); myuart_putc(' '); uart_int(1000+ub[i]); uart_newline(); wdt_reset(); } }
#endif

         R_DDR = 0;   // switch off low-side current after measurement
         uint16_t sumd = 0;
         sumd = sumabs8(ub+1,9);
         if (sumd > maxsumd) {
            maxsumd = sumd;
            dmax = d;
         }
         avg += sumd;
         wdt_reset();
#if ((DEB_UART & 0x10) != 0)
         myuart_putc('b'); myuart_putc(' '); uart_int(d); uart_int(sumd); uart_newline(); 
#endif
      }
#if ((DEB_UART & 0x10) != 0)
         myuart_putc('S'); myuart_putc(' '); uart_int(dmax); uart_int(maxsumd); uart_int(avg); uart_newline(); 
#endif
      if (ds==1) break;  // if we were already in the second (fine-grained) phase, break
                         // otherwise, set variables up for the second scan
      avg0 = avg>>8;       // compute twice the average amount of oscillation of the first scan; strictly we should divide avg by (d1-d0)/ds/2, but in the first scan that's 264, which we approximate by 256
      ds = 1;              // parameters for second scan: only a narrow range around the peak we found, in minimal steps
      d0 = dmax - 8;
      d1 = dmax + 8;
      if (d0&0x8000) { d0 = 0; d1 = 16; }
   }
   avg >>= 4;              // compute average; in the second scan, we should divide by d1-d0 = 16
//         myuart_putc('S'); myuart_putc(' '); uart_int(avg); uart_int(avg0); uart_newline(); 
   if (avg > avg0)
   {
      PartFound = PART_XTAL;
   }
   wht = what;
//         myuart_putc('S'); myuart_putc(' '); uart_int(dmax); uart_int(maxsumd); uart_int(avg); uart_newline(); 
//   wait_about5ms();
} /* end sampling_test_xtal */




uint8_t sampling_measure_xtal()
{
   byte LowPin = TP1;
   byte HighPin = TP3;
   byte HiPinR_L, LoADC;
   byte LoPinR_L;
   HiPinR_L = pinmaskRL(HighPin);
   LoPinR_L = pinmaskRL(LowPin);
   byte HiPinR_H = pinmaskRH(HighPin);
   LoADC = pinmaskADC(LowPin);

   int8_t uu[255];
   byte i;

   ADC_PORT = TXD_VAL;
   ADC_DDR = LoADC;			// switch Low-Pin to output (GND)
   wait100us();

   xtal_cancel = 0;

   ADMUX = HighPin|ADref1V1; // use built-in reference, about 1.1 V;
                             // that's enough, because peaks more than about 0.6 V are not of interest
                             // (because the negative peak would be chopped by the protection diodes)
   wait_aref_stabilize();                               

   lcd_next_line(0);         // do this here already because we'll be putting out progress markings in the course of the measurement


   // initial sampling, still with span=1
   samplingADC_freqgen((1<<smplADC_span)|wht, uu, 255, HiPinR_L, HiPinR_H, 0, HiPinR_L, dmax);
#if ((DEB_UART & 0x20) != 0)
   for (i=0;i<255;i++) { myuart_putc('a'); myuart_putc(' '); uart_int(uu[i]); uart_newline(); wdt_reset(); }
#endif

   uint16_t imax1 = findfreqmax(uu,255,6); // obtain a coarse estimate of the resonant frequency
                                           // start search at i=6, corresponding to 16 MHz / (256/6) = 375 kHz, to prevent DC component from being selected
   if (xtal_cancel) return xtal_cancel;

   // next, calculate span (here called ff) to be used for final measurement
   // for ceramic resonators, we choose ff about 16
   // for crystals, we first try about 64, check the amplitude at the end and go to about either 32 or 128 accordingly
   uint16_t imax2;
   byte ff = 64;
   byte oldff = 1;   // flag that we're still at the first ff attempt in case of crystal
   if (!isXtal) { oldff = 0; ff = 16; }

   byte u;
   byte ncumul = 1;
newff:;
   // I say "about" in the above, because we should avoid ff values which would result in a sampling rate near a subharmonic of the signal
   // if the signal would be close to a subharmonic, the information from the span=1 measurement above will not suffice to disambiguate the aliasing
   do {
      ff--;
      imax2 = imax1*ff;        // estimate of what the frequency will be with span=ff; note that truncation to 16 bits of this multiplication mimics the aliasing due to subsampling
      u = imax2>>8;
      if (ff == 1) break;      // one hopes we'll not get such a low ff; this happens with crystals very near half the clockfrequency; for those, we can't properly resolve the aliasing
   } while ((u&0x60) == 0 || (u&0x60) == 0x60);
   // the while condition avoids any case where u&0x7f is below 0x20 or above 0x59
   // i.e., we should expect between 32 and 95 periods in 256 samples
   // can use this in findfreqmax to reduce search time and to avoid false detections

   // sampling during longer time (with larger span), to make frequency estimate more precise
   for (i = ncumul; i != 0; i--) 
   {
      samplingADC_freqgen(((ff<<smplADC_span)|wht), uu, 255, HiPinR_L|LoPinR_L, HiPinR_H|LoPinR_L, 0|LoPinR_L,         HiPinR_L|LoPinR_L, dmax);
      wht |= samplingADC_cumul;
   }
   wht &= ~samplingADC_cumul;
   wdt_reset();
   // from here on, we use the test pins as follows:
   // low side is connected via de ADC-pin to ground (20 ohm or so) and via the low resistor (680 ohm) to +5V, thus establishing about 0.14 V on this pin
   // high side idles with 680 ohm to ground
   // high side gets pulses with 680 ohm to +5V
   // high side has 470 k to ground during measurement
   // additionally, the ADC input is switched to the low side when taking samples that are discarded, thus for every measurement "transfering" the 0.14 V to the high-side, thus establishing its DC level at said 0.14 V.
   R_DDR = 0;   // switch off low-side current after measurement

   show_progress();

   if (oldff) {
      // choose a more appropriate ff depending on the acquired data
      // smaller ff if at the end of the data the amplitude is small
      // bigger ff if at the end of the data the amplitude is still very large
      oldff = 0;
      int8_t umin = 127, umax = -127;
      for (i = 254;i > 220;i--) {
         int8_t u = uu[i];
         if (u < umin) umin = u;
         if (u > umax) umax = u;
      }
#if ((DEB_UART & 0x20) != 0)
      myuart_putc('O'); myuart_putc(' '); uart_int(umax); uart_int(umin); uart_int(ff); uart_newline();
#endif
      if ((umax-umin) < 10) { ff = 32; ncumul = 4; goto newff; }
      if ((umax-umin) > 40) { ff = 128; goto newff; }
   }


   imax2 = findfreqmax(uu,255,25); // find the frequency of the new measurement; 25 is a safe lower bound on expected frequency (32, see above)
   if (xtal_cancel) return xtal_cancel;
   if (u&0x80) imax2 =- imax2;    // make it negative if that's what we expect based on the coarse measurement
#if ((DEB_UART & 0x20) != 0)
   myuart_putc('o'); myuart_putc(' '); uart_int(dmax); uart_int(imax1); uart_int(imax2); uart_int(ff); uart_newline();
#endif


#if ((DEB_UART & 0x20) != 0)
   for (i=0;i<255;i++) { myuart_putc('a'); myuart_putc(' '); uart_int(uu[i]); uart_newline(); wdt_reset(); }
#endif

   // next step, but only for crystals, is to short-circuit the crystal for a while and see how that influences the phase
   // from that we can compute the series resonance frequency (which is the frequency in the short-circuited state)
   // and also disambiguate the aliasing due to our raw 16 MHz sample rate (i.e, distinguish between e.g. 7 and 9 MHz),
   // since we know the series resonance must be at a lower frequency

   uint8_t ssd = 0;    // how long the crystal is short-circuited, in units of (about) 256 clockcycles; after the measurement, it will hold how many of such units the crystal was short-circuited for
   uint8_t ph1l = 0;   // fractional part of total phase shift, unwrapped
   uint8_t ph1h = 0;   // integer part of same
   if (isXtal) {
      uint8_t probingstepsize = 2;  // flag: 2 initially, then 1 during increment probingstepsize, finally 0 when steps can no longer increase further
      uint8_t ssdstep = 1;
      uint8_t ph0 = ph0;         // starting phase, at ssd=5; self-initialization suppresses compiler warning
      uint8_t rphase;
      // we start with the longest possible short-circuit duration of the crystal
      // and then gradually try shorter durations
      // but not close to 0 duration, since esp. for low-frequency crystals, the phaseshift turns out to be not really linear in the duration
      // presumably, it takes a bit of time for the crystal's swinging frequency to adapt to the new load situation?
      // note: those low-freq crystals tend to get large ff values
      ssd = ff/2; 
      uint8_t sse = ssd/2;
      if (sse < 5) sse = 5; // ssd<5 is not allowed because the time it takes to send the pulses is included in the ssd time
      while (1)
      {
         for (i = ncumul; i != 0; i--) {
            samplingADC_freqgen_sck((ff<<smplADC_span)|wht|samplingADC_sck, uu, 255, HiPinR_L|LoPinR_L, HiPinR_H|LoPinR_L, 0|LoPinR_L, HiPinR_L|LoPinR_L, dmax, ssd);
            wht |= samplingADC_cumul;
         }
         wht &= ~samplingADC_cumul;
#if ((DEB_UART & 0x20) != 0)
if (ssd==5 || ssd==13)   for (i=0;i<255;i++) { myuart_putc('a'); myuart_putc(' '); uart_int(uu[i]); uart_int(ssd); uart_newline(); wdt_reset(); }
#endif
         R_DDR = 0;   // switch off low-side bias current between measurements
         rphase = findphase(uu+128,imax2,127);
         if (probingstepsize & 2) { 
            // first measurement, at maximum ssd; this is the 0-reference for the phase, stored in ph0
            ph0 = rphase; rphase = 0; 
            probingstepsize--;
         } else {
            // non-first measurement
            rphase -= ph0;
            if ((ph1l^rphase) & 0x80) {
               // most significant bit changed
               if (( (rphase-ph1l)^ph1l ) &0x80)
               {
                  // and we were near 0 or 255
                  // then change MSB accordingly, to "unwrap" the phase
                  if (ph1l&0x80) ph1h++; else ph1h--;
               }
            }
            if (probingstepsize & 1) {
               // if phase turns out to grow slowly, make stepsize larger
               int8_t r = rphase;
               if (r<48 && r>-48) ssdstep <<= 1;
               else probingstepsize = 0;
            }
         }	/* endif (probingstepsize & 2) */
         ph1l = rphase;

#if ((DEB_UART & 0x20) != 0)
         myuart_putc('y'); myuart_putc(' '); uart_int(ssd); uart_int(ph1l);  uart_int(ph1l+(((uint16_t)ph1h)<<8)); uart_int(ff); uart_int(dmax); uart_int(imax1); uart_int(imax2); uart_int(ph0+rphase); uart_newline(); 
#endif
         wdt_reset();
         if (!(RST_PIN_REG & (1<<RST_PIN))) return 1;
         show_progress();
         if (ssd < (ssdstep+sse)) break;
         ssd -= ssdstep;
      }		/* end while (1) */
      ssd = (ff / 2) - ssd;
      // now ph1h:ph1l is the phaseshift (in 360/256 degrees) during ssd*256 clockcycles of shortcircuiting the crystal
      // this implies a frequency difference of ph1/256 / (ssd*256/fclock) = ph1/256/ssd/256*fclock = ph1 * fclock / ssd / 65536
      // assuming fclk = 16.000000000 MHz: ph1 * 244.140 / ssd

   }
   else ph1h = 0x00;  // for ceramic resonators; tells the below code not to do alias-correction, i.e., not to subtract the measured freq from 16. This is of course a guess, in case of cer.res. we have no way reliably resolve the aliasing.

   uint32_t frq;
   // samplerate is fclk/ff
   // so frequency is fclk/ff * imax2/65536 = imax2 * (fclk/65536/ff)
   // but that's after subsampling by a factor ff
   // so an integer number of times fclk/ff needs to be added
   // the real frequency must be "near" imax1/65536*fclk = fclk/65536/ff * imax1*ff
   frq = imax2;
   while (frq+32768 < (uint32_t)imax1*ff) frq += 65536ul;   // older version, takes more flash
//   while (((frq+32768)>>8) < (imax1>>8)*ff) frq += 65536ul;    // hmm, but now, a month later and after other changes, the old version takes less flash; weird
#if (F_CPU > 15900000) && (F_CPU < 16100000)
   // assuming fclk = 16.000000000 MHz:
   //      frq = frq*16000000/65536/ff;
   //      frq = frq*15625/64/ff;
   frq = (((frq*625)/64)*25)/ff;

//#define F_CPU 16016000UL
//#define F_CPU 16032000UL
//#define F_CPU 15968000UL
 #if F_CPU!=16000000UL        // but... unfortunately the makefile system doesn't yet support non-round CPU clocks
   // correction for the CPU clock deviating from 16 MHz
   #if (F_CPU>16000000UL)
      uint16_t df = (F_CPU-16000000UL)*256/244;
      df = ( (frq>>16) * df )>>8;
      frq += df;
   #else
      uint16_t df = (16000000UL-F_CPU)*256/244;
      df = ( (frq>>16) * df )>>8;
      frq -= df;
   #endif
 #endif
#endif
#if F_CPU == 20000000
   // assuming fclk = 20.000000000 MHz:
   //      frq = f*20000000/65536/ff;
   //      frq = f*78125/256/ff;
   frq = (((frq*3125)/256)*25)/ff;
#endif
   uint16_t ph1 = (((uint16_t)ph1h)<<8) | ph1l;
   if ((ph1h&0x80)) { frq = F_CPU-frq; ph1 = -ph1; }


   if (isXtal && ff!=1) {

      // in case of crystal, find motional capacitance and series resonance
      //
      // note: in principle we could find even more parameters of the crystal's equivalent circuit:
      // - motional inductance, calculated from mot.cap. and ser.res. freq
      // - parallel capacitance: simply to standard cap. measurement
      // - series resistance: could in theory be computed from Q, found from decay rate of oscillation, but it turns out the tester circuit damps the crystal more.
      // I (pa3fwm) didn't implement these, because it saves program memory space (the ATmega328 is getting full),
      //    the quantities are rarely needed, and mot.ind. can be computed by user, and par.cap. can be measured in [C] mode.

//      uint16_t df = ph1 * (244 / ssd);   // frequency shift due to short-circuiting
      uint16_t df = ( (ph1 * 62) / ssd )<<2;   // frequency shift due to short-circuiting
      frq -= df;                           // compute series resonant frequency
      uint16_t parcap;
      parcap = sampling_cap(HighPin, LowPin, 2);  // capacitance including parasitics, in 0.01pF, needed to compute Cm
      parcap -= 1400;    // subtract 14 pF of capacitance in the on-chip S&H, because this is behind >=12k of resistance, so invisible at higher frequencies; below about 3 MHz, we should perhaps subtract only 10 pF, or rather, do a gradual transition...
        // motional capacitance in fF = parcap (in cpF) * df/frq * 20
//      uint16_t cm = ((uint32_t)parcap * (df / 10))/(uint16_t)(frq/200);   // motional capacitance in fF
      uint16_t cm = ((parcap/25) * (df>>5) )/(uint16_t)(frq>>14);    // try to keep it in a 16-bit division, at the expense of accuracy; this measurement isn't very precise anyway
      // 25 should precisely be 25.6; rounded down to at least slightly compensate for rounding down due to truncation

      lcd_set_cursor(0,9);
      static const unsigned char str_Cm[] MEM_TEXT = "Cm="; lcd_MEM_string(str_Cm);
      DisplayValue(cm,-15,'F',2);

      lcd_next_line(0);
      static const unsigned char str_ser[] MEM_TEXT = "ser "; lcd_MEM_string(str_ser);
      Display_Hz(frq, 6); 	// DisplayValue(f,0,'H',6); lcd_data('z');

      frq += df;   // back to parallel resonant frequency

      lcd_next_line_wait(0);
      static const unsigned char str_par[] MEM_TEXT = "par "; lcd_MEM_string(str_par);

      Display_Hz(frq, 6); 	// DisplayValue(frq,0,'H',6); lcd_data('z');
   } else {
      // no crystal, then just ceramic resonator, only show a single resonant frequency
      // also use this code for crystals if ff=1, practically speaking, if we have an 8 MHz crystal, since then precision measurements are not possible
      lcd_clear_line2();
      Display_Hz(frq, 6); 	// DisplayValue(frq,0,'H',6); lcd_data('z');
   }

//   lcd_refresh();
   return xtal_cancel;
} /* end sampling_measure_xtal() */


#endif   // WITH_XTAL
