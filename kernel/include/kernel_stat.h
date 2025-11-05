#ifndef _KSTAT_H_
#define _KSTAT_H_

#include <x86/interrupt.h>
#include <stddef.h>

/*
 * Kernel statistics.
 */
struct kernel_stat {
	uint32_t	irqs[NR_IRQS];
	uint32_t	context_switch;
};

extern struct kernel_stat kstat;

#endif
