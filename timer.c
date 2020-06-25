#include <ps2sdkapi.h>
#include "launchelf.h"

// Timer Count
u64 Timer(void)
{
	return ps2_clock() / (PS2_CLOCKS_PER_SEC / CLOCKS_PER_SEC);
}
