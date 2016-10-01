#include "launchelf.h"

// Timer Define
#define T0_COUNT ((volatile unsigned long *)0x10000000)
#define T0_MODE ((volatile unsigned long *)0x10000010)

static int TimerInterruptID = -1;
static u64 TimerInterruptCount = 0;

// Timer Interrupt
int TimerInterrupt(int a)
{
    TimerInterruptCount++;
    *T0_MODE |= (1 << 11);
    return -1;
}
// Timer Init
void TimerInit(void)
{
    *T0_MODE = 0x0000;
    TimerInterruptID = AddIntcHandler(9, TimerInterrupt, 0);
    EnableIntc(9);
    *T0_COUNT = 0;
    *T0_MODE = (2 << 0) + (1 << 7) + (1 << 9);
    TimerInterruptCount = 0;
}
// Timer Count
u64 Timer(void)
{
    u64 ticks = (*T0_COUNT + (TimerInterruptCount << 16)) * (1000.0F / (147456000.0F / 256.0F));
    return ticks;
}
// Timer End
void TimerEnd(void)
{
    *T0_MODE = 0x0000;
    if (TimerInterruptID >= 0) {
        DisableIntc(9);
        RemoveIntcHandler(9, TimerInterruptID);
        TimerInterruptID = -1;
    }
    TimerInterruptCount = 0;
}
