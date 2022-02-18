#include <time.h>
#include "launchelf.h"

// Timer Count
u64 Timer(void)
{
    return (clock() / (CLOCKS_PER_SEC / 1000));
}
