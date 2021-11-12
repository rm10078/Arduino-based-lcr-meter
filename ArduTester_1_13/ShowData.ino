//#include "Transistortester.h"
//#include "Makefile.h" //J-L

/*  ShowData shows the Software version number and */
/*  the calibration data at the 2-line or 4-line LCD */
#ifdef WITH_MENU
void ShowData(void) {
#ifdef SamplingADC
  uint8_t ii,jj,kk;
  uint16_t cc,dd;
#endif

#ifdef WITH_ROTARY_SWITCH
show_page_1:
#endif
  lcd_clear();
  lcd_MEM_string(VERSION_str);	// "Version x.xxk"
  lcd_line2();
  lcd_MEM2_string(R0_str);	// "R0="
  DisplayValue16(eeprom_read_byte(&EE_ESR_ZEROtab[2]),-2,' ',3);
  DisplayValue16(eeprom_read_byte(&EE_ESR_ZEROtab[3]),-2,' ',3);
  DisplayValue16(eeprom_read_byte(&EE_ESR_ZEROtab[1]),-2,LCD_CHAR_OMEGA,3);
#if (LCD_LINES > 3)
  lcd_line3();
#else
  wait_for_key_ms(MIDDLE_WAIT_TIME);
 #ifdef WITH_ROTARY_SWITCH
  if (rotary.incre > FAST_ROTATION) return;	// fast rotation ends the function
  if (rotary.count < 0) goto show_page_1;
show_page_2:
 #endif
  lcd_clear();
#endif
  /* output line 3 */
  lcd_MEM_string(RIHI_str); // "RiHi="
  DisplayValue16(RRpinPL,-1,LCD_CHAR_OMEGA,3);
#if (LCD_LINES > 3)
  lcd_line4();
#else
  lcd_line2();
#endif
  /* output line 4 */
  lcd_MEM_string(RILO_str); // "RiLo="
  DisplayValue16(RRpinMI,-1,LCD_CHAR_OMEGA,3);

  wait_for_key_ms(MIDDLE_WAIT_TIME);
#ifdef WITH_ROTARY_SWITCH
  if (rotary.incre > FAST_ROTATION) return;	// fast rotation ends the function
#if (LCD_LINES > 3)
  if (rotary.count < 0) goto show_page_1;
 #else
  if (rotary.count < -1) goto show_page_1;
  if (rotary.count < 0) goto show_page_2;
 #endif
show_page_3:
#endif
  lcd_clear();
  lcd_MEM_string(C0_str);                       //output "C0 "
  u2lcd_space(eeprom_read_byte(&c_zero_tab[5]) -(COMP_SLEW1 / (CC0 + CABLE_CAP + COMP_SLEW2)));		//output cap0 1:3
  u2lcd_space(eeprom_read_byte(&c_zero_tab[6]) -(COMP_SLEW1 / (CC0 + CABLE_CAP + COMP_SLEW2)));		//output cap0 2:3
  DisplayValue16(eeprom_read_byte(&c_zero_tab[2]) -(COMP_SLEW1 / (CC0 + CABLE_CAP + COMP_SLEW2)) ,-12,'F',3);		//output cap0 1:2
  lcd_line2();
  lcd_spaces(3);
  u2lcd_space(eeprom_read_byte(&c_zero_tab[1]) -(COMP_SLEW1 / (CC0 + CABLE_CAP + COMP_SLEW2)));		//output cap0 3:1
  u2lcd_space(eeprom_read_byte(&c_zero_tab[4]) -(COMP_SLEW1 / (CC0 + CABLE_CAP + COMP_SLEW2)));		//output cap0 3:2
  DisplayValue16(eeprom_read_byte(&c_zero_tab[0]) -(COMP_SLEW1 / (CC0 + CABLE_CAP + COMP_SLEW2)) ,-12,'F',3);		//output cap0 2:1
#if (LCD_LINES > 3)
  lcd_line3();
#else
  wait_for_key_ms(MIDDLE_WAIT_TIME);
 #ifdef WITH_ROTARY_SWITCH
  if (rotary.incre > FAST_ROTATION) return;	// fast rotation ends the function
  if (rotary.count < -2) goto show_page_1;
  if (rotary.count < -1) goto show_page_2;
  if (rotary.count < 0) goto show_page_3;
show_page_4:
 #endif
  lcd_clear();
#endif
  /* output line 7 */
  lcd_MEM2_string(REF_C_str);	// "REF_C="
  i2lcd((int16_t)eeprom_read_word((uint16_t *)(&ref_offset)));
#if (LCD_LINES > 3)
  lcd_line4();
#else
  lcd_line2();
#endif
  /* output line 8 */
  lcd_MEM2_string(REF_R_str);  // "REF_R="
  i2lcd((int8_t)eeprom_read_byte((uint8_t *)(&RefDiff)));

#ifdef SamplingADC
#ifdef WITH_ROTARY_SWITCH
show_page_5:
#endif
  /* modified output from sampling_cap_calibrate */
  lcd_set_cursor((LCD_LINES-1)*PAGES_PER_LINE,0);      // set for initial clear screen
  for (ii=0;ii<=2;ii++)
    for (jj=0;jj<=2;jj++)
       if (ii != jj) {
          kk = (ii * 3) + jj - 1;
          cc = eeprom_read_word((void *)(c_zero_tab2_lo+kk));
          dd = eeprom_read_word((void *)(c_zero_tab2_hi+kk));
          if (lcd_text_line > (LCD_LINES - 2)) {
             wait_for_key_ms(MIDDLE_WAIT_TIME);
 #ifdef WITH_ROTARY_SWITCH
             if (rotary.incre > FAST_ROTATION) return;	// fast rotation ends the function
  #if (LCD_LINES > 3)
             if (rotary.count < -2) goto show_page_1;	// page_2 is shown together with _page_1
             if (rotary.count < -1) goto show_page_3;	// page_4 is shown together with _page 3
             if (rotary.count < 0) goto show_page_5;
  #else
             if (rotary.count < -4) goto show_page_1;
             if (rotary.count < -3) goto show_page_2;
             if (rotary.count < -2) goto show_page_3;
             if (rotary.count < -1) goto show_page_4;
             if (rotary.count < 0) goto show_page_5;
  #endif
 #endif  /* WITH_ROTARY_SWITCH */
             lcd_clear();
          } else {  
	     lcd_next_line(0);
          }
          lcd_MEM_string(C0samp_str);	// "C0samp "
	  lcd_testpin(ii);
	  lcd_data(':');
          lcd_testpin(jj);
	  lcd_next_line(0);
	  DisplayValue16(cc,-2,' ',3);
	  DisplayValue16(dd,-12-2,'F',3);
       }
#else  /* without SamplingADC */
 #ifdef WITH_ROTARY_SWITCH
  wait_for_key_ms(MIDDLE_WAIT_TIME);
  if (rotary.incre > FAST_ROTATION) return;	// fast rotation ends the function
  #if (LCD_LINES > 3)
  if (rotary.count < -1) goto show_page_1;
  if (rotary.count < 0) goto show_page_3;
  #else
  if (rotary.count < -3) goto show_page_1;
  if (rotary.count < -2) goto show_page_2;
  if (rotary.count < -1) goto show_page_3;
  if (rotary.count < 0) goto show_page_4;
  #endif
 #endif

#endif  /* SamplingADC */
  wait_for_key_ms(MIDDLE_WAIT_TIME);
//#if  defined(WITH_GRAPHICS) && !defined(SamplingADC)
#ifdef SHOW_ICONS
 ShowIcons();		// show all Icons
#endif
}
#endif
