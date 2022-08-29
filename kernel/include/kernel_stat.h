#ifndef _KSTAT_H_
#define _KSTAT_H_

#include <stddef.h>

/*
 * Kernel statistics.
 */
struct kernel_stat_t {
	uint32_t	interrupts;
	uint32_t	context_switch;
};

extern struct kernel_stat_t kstat;

#endif
