/*
  wiring.c - Partial implementation of the Wiring API for the ATmega8.
  Part of Arduino - http://www.arduino.cc/

  Copyright (c) 2005-2006 David A. Mellis

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General
  Public License along with this library; if not, write to the
  Free Software Foundation, Inc., 59 Temple Place, Suite 330,
  Boston, MA  02111-1307  USA

  $Id: wiring.c 461 2008-07-02 19:06:27Z mellis $
*/

#include "wiring_private.h"
#include "osccal.h"

volatile unsigned long timer0_clock_cycles = 0;
volatile unsigned long timer0_millis = 0;

SIGNAL(SIG_OVERFLOW0)
{
	// timer 0 prescale factor is 64 and the timer overflows at 256
	timer0_clock_cycles += 64UL * 256UL;
	while (timer0_clock_cycles > clockCyclesPerMicrosecond() * 1000UL) {
		timer0_clock_cycles -= clockCyclesPerMicrosecond() * 1000UL;
		timer0_millis++;
	}
}

unsigned long millis()
{
	unsigned long m;
	uint8_t oldSREG = SREG;
	
	// disable interrupts while we read timer0_millis or we might get an
	// inconsistent value (e.g. in the middle of the timer0_millis++)
	cli();
	m = timer0_millis;
	SREG = oldSREG;
	
	return m;
}

void delay(unsigned long ms)
{
	unsigned long start = millis();
	
	while (millis() - start <= ms)
		;
}

/* Delay for the given number of microseconds.  Assumes a 8 or 16 MHz clock. 
 * Disables interrupts, which will disrupt the millis() function if used
 * too frequently. */
void delayMicroseconds(unsigned int us)
{
	uint8_t oldSREG;

	// calling avrlib's delay_us() function with low values (e.g. 1 or
	// 2 microseconds) gives delays longer than desired.
	//delay_us(us);

#if F_CPU >= 16000000L
	// for the 16 MHz clock on most Arduino boards

	// for a one-microsecond delay, simply return.  the overhead
	// of the function call yields a delay of approximately 1 1/8 us.
	if (--us == 0)
		return;

	// the following loop takes a quarter of a microsecond (4 cycles)
	// per iteration, so execute it four times for each microsecond of
	// delay requested.
	us <<= 2;

	// account for the time taken in the preceeding commands.
	us -= 2;
#else
	// for the 8 MHz internal clock on the ATmega168

	// for a one- or two-microsecond delay, simply return.  the overhead of
	// the function calls takes more than two microseconds.  can't just
	// subtract two, since us is unsigned; we'd overflow.
	if (--us == 0)
		return;
	if (--us == 0)
		return;

	// the following loop takes half of a microsecond (4 cycles)
	// per iteration, so execute it twice for each microsecond of
	// delay requested.
	us <<= 1;
    
	// partially compensate for the time taken by the preceeding commands.
	// we can't subtract any more than this or we'd overflow w/ small delays.
	us--;
#endif

	// disable interrupts, otherwise the timer 0 overflow interrupt that
	// tracks milliseconds will make us delay longer than we want.
	oldSREG = SREG;
	cli();

	// busy wait
	__asm__ __volatile__ (
		"1: sbiw %0,1" "\n\t" // 2 cycles
		"brne 1b" : "=w" (us) : "0" (us) // 2 cycles
	);

	// reenable interrupts.
	SREG = oldSREG;
}

void CLKPR_Calibrate(void)
{
  // The Butterfly has an internal 8MHz clock. If F_CPU is not 8MHz then
  // the clock prescale can be used to divide down the internal clock. This
  // tries to get as close to the target frequency as possible.
 
  int clkprx = 0;
  // 8000000 is Butterfly internal clock
  // B1000 is maximum allowed scale
  while (( (8000000/(1<<clkprx) ) > F_CPU) && (clkprx < B1000))
    clkprx++;

  if ((clkprx < B1000)   // B1000 is as low as it goes, leave it. 
   && (clkprx > B0000))  // B0000 is as high as it goes, leave it.  
  {
    // Check whether clkprx or clkprx-1 is closer to the target
    if ( (F_CPU-(8000000/(1<<clkprx))) > ((8000000/(1<<(clkprx-1)))-F_CPU)){
      clkprx--;
    }
  }
  
//#ifdefined(__AVR_ATmega169__)
	// Reset clock prescale
	CLKPR = (1 << CLKPCE);
    CLKPR = clkprx;
//#   
}

void init()
{
    // Set up the clock prescale first
    CLKPR_Calibrate();
    // Then trim the RC oscillator to get as close to the 
    // requested clock frequency as possible. 
    OSCCAL_Calibrate();
    
	// this needs to be called before setup() or some functions won't
	// work there
	sei();
		
	// on the ATmega168, timer 0 is also used for fast hardware pwm
	// (using phase-correct PWM would mean that timer 0 overflowed half as often
	// resulting in different millis() behavior on the ATmega8 and ATmega168)
//#if defined(__AVR_ATmega168__) 
//	sbi(TCCR0A, WGM01);
//	sbi(TCCR0A, WGM00);
//#elseif defined(__AVR_ATmega169__)
	sbi(TCCR0A, WGM01);
	sbi(TCCR0A, WGM00);
//#endif  
	// set timer 0 prescale factor to 64
//#if defined(__AVR_ATmega168__)
//	sbi(TCCR0B, CS01);
//	sbi(TCCR0B, CS00);
//#elseif defined(__AVR_ATmega169__)
	sbi(TCCR0A, CS01);
	sbi(TCCR0A, CS00);
//#else
//	sbi(TCCR0, CS01);
//	sbi(TCCR0, CS00);
//#endif
	// enable timer 0 overflow interrupt
//#if defined(__AVR_ATmega168__)
//	sbi(TIMSK0, TOIE0);
//#elseif defined(__AVR_ATmega169__)
	sbi(TIMSK0, TOIE0);
//#else
//	sbi(TIMSK, TOIE0);
//#endif

	// timers 1 and 2 are used for phase-correct hardware pwm
	// this is better for motors as it ensures an even waveform
	// note, however, that fast pwm mode can achieve a frequency of up
	// 8 MHz (with a 16 MHz clock) at 50% duty cycle

	// set timer 1 prescale factor to 64
	sbi(TCCR1B, CS11);
	sbi(TCCR1B, CS10);
	// put timer 1 in 8-bit phase correct pwm mode
	sbi(TCCR1A, WGM10);

	// set timer 2 prescale factor to 64
//#if defined(__AVR_ATmega168__)
//	sbi(TCCR2B, CS22);	
//#elseif defined(__AVR_ATmega169__)	
	sbi(TCCR2A, CS22);
//#else
//	sbi(TCCR2, CS22);
//#endif
	// configure timer 2 for phase correct pwm (8-bit)
//#if defined(__AVR_ATmega168__)
//	sbi(TCCR2A, WGM20);
//#elseif defined(__AVR_ATmega169__)	
	sbi(TCCR2A, WGM20);
//#else
//	sbi(TCCR2, WGM20);
//#endif

	// Find an A2D prescale that is <= 200KHz
	int adpsx = 0;
	while ((F_CPU/(1<<++adpsx))>200000)
	   ;
    ADCSRA = (ADCSRA&~(1<<ADPS0|1<<ADPS0|1<<ADPS0)) | adpsx;   

	// enable a2d conversions
	sbi(ADCSRA, ADEN);

	// the bootloader connects pins 0 and 1 to the USART; disconnect them
	// here so they can be used as normal digital i/o; they will be
	// reconnected in Serial.begin()
//#if defined(__AVR_ATmega168__)
//	UCSR0B = 0;
//#elseif defined(__AVR_ATmega169__)	
	UCSRB = 0;
//#else
//	UCSRB = 0;
//#endif
}
