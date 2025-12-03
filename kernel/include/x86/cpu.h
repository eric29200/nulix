#ifndef _CPU_H_
#define _CPU_H_

#include <stddef.h>

#define X86_FEATURE_TSC		0x00000010	/* Time Stamp Counter */

void init_cpu();
size_t get_cpuinfo(char *page);

#endif