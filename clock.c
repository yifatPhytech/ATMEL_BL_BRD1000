#include "define.h"
#include "clock.h"

volatile unsigned short ms100;

void clock_interrupt_100ms()
{
    ms100++;
}

void delay_ms(unsigned short msecs)
{
    unsigned short ticks = ms100;
    msecs /= 100;
    
    while ((ms100 - ticks) < msecs)
    {
	    WATCHDOG_RESET();
    }
}

void delay_secs(unsigned short secs)
{
	unsigned short ticks = ms100;
	secs *= 10;
	
	while ((ms100 - ticks) < secs)
	{
		WATCHDOG_RESET();
	}
}
