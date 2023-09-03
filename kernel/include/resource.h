#ifndef _RESOURCE_H_
#define _RESOURCE_H_

#include <stddef.h>
#include <time.h>

#define RUSAGE_SELF		0
#define RUSAGE_CHILDREN		-1
#define RUSAGE_BOTH		-2

#define RLIMIT_CPU		0		/* CPU time in ms */
#define RLIMIT_FSIZE		1		/* Maximum filesize */
#define RLIMIT_DATA		2		/* max data size */
#define RLIMIT_STACK		3		/* max stack size */
#define RLIMIT_CORE		4		/* max core file size */
#define RLIMIT_RSS		5		/* max resident set size */
#define RLIMIT_NPROC		6		/* max number of processes */
#define RLIMIT_NOFILE		7		/* max number of open files */
#define RLIMIT_MEMLOCK		8		/* max locked-in-memory address space */

#define RLIM_NLIMITS		9

/*
 * Process usage statistics.
 */
struct rusage {
	struct timeval		ru_utime;	/* user CPU time used */
	struct timeval		ru_stime; 	/* system CPU time used */
	long			ru_maxrss;	/* maximum resident set size */
	long			ru_ixrss;	/* integral shared memory size */
	long			ru_idrss;	/* integral unshared data size */
	long			ru_isrss;	/* integral unshared stack size */
	long			ru_minflt;	/* page reclaims (soft page faults) */
	long			ru_majflt;	/* page faults (hard page faults) */
	long			ru_nswap;	/* swaps */
	long			ru_inblock;	/* block input operations */
	long			ru_oublock;	/* block output operations */
	long			ru_msgsnd;	/* IPC messages sent */
	long			ru_msgrcv;	/* IPC messages received */
	long			ru_nsignals;	/* signals received */
	long			ru_nvcsw;	/* voluntary context switches */
	long			ru_nivcsw;	/* involuntary context switches */
};

/*
 * Resource limit.
 */
struct rlimit {
	uint32_t		rlim_cur;	/* soft limit */
	uint32_t		rlim_max;	/* hard limit */
};

/*
 * Resource limit.
 */
struct rlimit64 {
	uint64_t		rlim_cur;	/* soft limit */
	uint64_t		rlim_max;	/* hard limit */
};

#endif
