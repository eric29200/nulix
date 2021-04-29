#ifndef _DELAY_H_
#define _DELAY_H_

#include <stddef.h>

#define ms_to_jiffies(ms)          (((uint32_t) (ms) + (1000L / HZ) - 1) / (1000L / HZ))

void msleep(uint32_t msecs);

#endif
