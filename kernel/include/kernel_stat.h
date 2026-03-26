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
	uint32_t	pswpin;
	uint32_t	pswpout;
	time_t		cpu_user;
	time_t		cpu_system;
	time_t		cpu_nice;
};

extern struct kernel_stat kstat;

#endif
