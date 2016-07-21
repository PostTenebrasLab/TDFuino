/*
    TDFuino

    Copyright 2016 magnustron

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


/* PINOUT */
/* PD6 = AN0 = 6 = signal */
/* PD7 = AN1 = 7 = reference*/

/* DISPLAY */
#define LCD_I2C_ADDR 0x27
#define LED_PIN  3
#define EN_PIN   2
#define RW_PIN   1
#define RS_PIN   0
#define D4_PIN   4
#define D5_PIN   5
#define D6_PIN   6
#define D7_PIN   7
#define LED_POL POSITIVE

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
LiquidCrystal_I2C lcd(LCD_I2C_ADDR,EN_PIN,RW_PIN,RS_PIN,D4_PIN,D5_PIN,D6_PIN,D7_PIN,LED_PIN,LED_POL);

#define F_IF 455000UL
#define NPERIODS 1500 /* FIXME: NPERIODS*F_CPU/F_IF must be less than 2^16 (with margin for IF uncertainty) */
#define N1S   ( (F_IF       +(NPERIODS+1)/2)/NPERIODS)
#define N25MS (((F_IF+20)/40+(NPERIODS+1)/2)/NPERIODS)
#define NDTS  N1S

#define FLAG      GPIOR0 /* needs to be 0 at startup */
#define TIMEL     OCR0A  /* hijack the PWM (not used) */
#define TIMEH     OCR0B  /* hijack the PWM (not used) */
#define ISRTMPIO0 EEDR   /* hijack the EEPROM (not used) */
#define ISRTMPIO1 EEAR   /* hijack the EEPROM (not used) */
#define POSTL     GPIOR1 /* needs to be 0 at startup */
#define POSTH     GPIOR2 /* needs to be 0 at startup */
ISR(TIMER1_CAPT_vect,ISR_NAKED) {
  asm volatile(
    /*           ISR response time: +4,+5,+6 (dependent on instruction currently being active) */
    /*         jmp (not rjmp) here:       +3 */
    "  out   %[tmpio0],r16         \n" /* +1 */
    "  in    r16,__SREG__          \n" /* +1 */
    "  out   %[tmpio1],r16         \n" /* +1 */
    "  in    r16,%[postl]          \n" /* +1 */
    "  subi  r16,1                 \n" /* +1 */
    "  out   %[postl],r16          \n" /* +1 */
    "  brcs  isr_next              \n" /* +1/+2 */
    "  in    r16,%[tmpio1]         \n" /* +1/-- */
    "  out   __SREG__,r16          \n" /* +1/-- */
    "  in    r16,%[tmpio0]         \n" /* +1/-- */
    "  reti                        \n" /* +4/-- */
    "isr_next:                     \n" 
    "  in    r16,%[posth]          \n" /* --/+1 */
    "  subi  r16,1                 \n" /* --/+1 */
    "  brcs  isr_action            \n" /* --/+1/+2 */
    "  out   %[posth],r16          \n" /* --/+1/-- */
    "  in    r16,%[tmpio1]         \n" /* --/+1/-- */
    "  out   __SREG__,r16          \n" /* --/+1/-- */
    "  in    r16,%[tmpio0]         \n" /* --/+1/-- */
    "  reti                        \n" /* --/+4/-- */
    "isr_action:                   \n" 
    "  out   %[flag],r16           \n" /* --/--/+1 r16=0xFF */
    "  ldi   r16,%[nh]             \n" /* --/--/+1 */
    "  out   %[posth],r16          \n" /* --/--/+1 */
    "  ldi   r16,%[nl]             \n" /* --/--/+1 */
    "  out   %[postl],r16          \n" /* --/--/+1 */
    "  lds   r16,%[icr1l]          \n" /* --/--/+2 */
    "  out   %[timel],r16          \n" /* --/--/+1 */
    "  lds   r16,%[icr1h]          \n" /* --/--/+2 */
    "  out   %[timeh],r16          \n" /* --/--/+1 */
    "  in    r16,%[tmpio1]         \n" /* --/--/+1 */
    "  out   __SREG__,r16          \n" /* --/--/+1 */
    "  in    r16,%[tmpio0]         \n" /* --/--/+1 */
    "  reti                        \n" /* --/--/+4 */
                                       /* 23/28/38 (max values; out of 32 on average) */
    :
    :[icr1l]    "M" (_SFR_MEM_ADDR(ICR1L))
    ,[icr1h]    "M" (_SFR_MEM_ADDR(ICR1H))
    ,[nl]       "M" ((NPERIODS-1)>>0&0xFF)
    ,[nh]       "M" ((NPERIODS-1)>>8&0xFF)
    ,[tmpio0]   "I" (_SFR_IO_ADDR(ISRTMPIO0))
    ,[tmpio1]   "I" (_SFR_IO_ADDR(ISRTMPIO1))
    ,[flag]     "I" (_SFR_IO_ADDR(FLAG))
    ,[postl]    "I" (_SFR_IO_ADDR(POSTL))
    ,[posth]    "I" (_SFR_IO_ADDR(POSTH))
    ,[timel]    "I" (_SFR_IO_ADDR(TIMEL))
    ,[timeh]    "I" (_SFR_IO_ADDR(TIMEH))
    :
  );
}

void printchar(char c) {
  while (!(UCSR0A&1<<UDRE0));
  UDR0=c;
}

void printstr(const char *str) {
  char c;
  while (c=*(str++)) printchar(c);
}

void printhex(uint8_t x) {
  char c;
  c='0'|x>>4&0x0F;
  if (c>'9') c=c-10-'0'+'A';
  printchar(c);
  c='0'|x>>0&0x0F;
  if (c>'9') c=c-10-'0'+'A';
  printchar(c);
}

void setup() {
  noInterrupts();
  // Timer/Counter 0 (default time base):
  TIMSK0 =0x00; // disable interrupts
  // Analog comparator:
  ADCSRB =0x40; // use ADC multiplexer for negative input (FIXME)
  ACSR   =0x04; // use to capture Timer/Counter 1, no interrupts
  // Timer/Counter 1:
  TIMSK1 =0x20; // interrupts on capture
  TCCR1A =0x00; // disable any PWM out
  TCCR1B =0x01; // falling edge, no noise canceling, full speed
  // I/O:
  Serial.begin(115200); // only used for baud rate etc.
  UCSR0B&=0x1F; // disable interrups

  //lcd.begin(16,2);

  interrupts();
}

void loop() {
  static uint16_t dts[NDTS]={0};
  static uint16_t lastt =0;
  static uint32_t dt1s  =0;
  static uint32_t dt25ms=0;
  static uint32_t dtlast=0;
  static uint16_t wptr=0;
  if (FLAG) {
    FLAG=0;
    uint16_t t=TIMEH<<8|TIMEL;
    uint16_t dt=t-lastt;
    lastt=t;
    uint16_t i1s  =wptr-N1S  ;
    uint16_t i25ms=wptr-N25MS;
    if (i1s  >=NDTS) i1s  +=NDTS;
    if (i25ms>=NDTS) i25ms+=NDTS;
    dt1s  -=dts[i1s  ];
    dt25ms-=dts[i25ms];
    dt1s  +=dt;
    dt25ms+=dt;
    dts[wptr]=dt;
    ++wptr;
    if (wptr==NDTS) wptr=0;
    // vote
    int32_t delta=dt25ms*N1S-dt1s*N25MS;
    printhex(dt1s  >>24&0xFF);
    printhex(dt1s  >>16&0xFF);
    printhex(dt1s  >> 8&0xFF);
    printhex(dt1s  >> 0&0xFF);
    printchar('-');
    printhex(dt25ms>>24&0xFF);
    printhex(dt25ms>>16&0xFF);
    printhex(dt25ms>> 8&0xFF);
    printhex(dt25ms>> 0&0xFF);
    printchar('~');
    printhex(delta>>24&0xFF);
    printhex(delta>>16&0xFF);
    printhex(delta>> 8&0xFF);
    printhex(delta>> 0&0xFF);
    printchar('\n');
   // lcd.clear();
   // lcd.setCursor(0,0);
   // lcd.print("DT = ");
   // lcd.print(dt,1);
   // lcd.print(" clks");
   // lcd.setCursor(0,1);
   // lcd.print("f  = ");
   // lcd.print(1000.0*F_CPU*(N_SKIP+N_CAPTURE)/dt,1);
   // lcd.print(" kHz");
    if (FLAG) {
      printstr("OVERFLOW\n");
    }
  }
}

