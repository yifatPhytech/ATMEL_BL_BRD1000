#ifndef __CLOCK_H__
#define __CLOCK_H__

extern volatile unsigned short ms100;

#define clock_getTicks() (ms100)
#define clock_tickDiff(ticks) (ms100 - ticks)
#define clock_tickDiffSecs(ticks) ((ms100 - ticks) / 10)

void clock_interrupt_100ms();
void delay_secs(unsigned short secs);
void delay_ms(unsigned short msecs);

#endif
