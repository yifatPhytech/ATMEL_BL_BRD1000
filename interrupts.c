#include "define.h"
#include "clock.h"
#include "avr/interrupt.h"

extern unsigned int wdtSecs;

// Timer1 output compare A interrupt service routine - EVERY 100 ML SECOND
ISR(TIMER1_COMPA_vect)
{
    clock_interrupt_100ms();
}

// Watchdog timer to wakeup bootloader every 8secs
ISR(WDT_vect)
{
	wdtSecs += 8;
}
