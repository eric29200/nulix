#ifndef _SYSINFO_H_
#define _SYSINFO_H_

/*
 * System info structure.
 */
struct sysinfo_t {
	long		uptime;
	unsigned long	loads[3];
	unsigned long	totalram;
	unsigned long	freeram;
	unsigned long	sharedram;
	unsigned long	bufferram;
	unsigned long	totalswap;
	unsigned long	freeswap;
	unsigned short	procs;
	unsigned long	totalhigh;
	unsigned long	freehigh;
	unsigned int	mem_unit;
};

#endif
