#ifndef _TIME_H_
#define _TIME_H_

#include <stddef.h>

extern time_t startup_time;
extern volatile time_t jiffies;
extern struct kernel_timeval xtimes;

#define HZ				1000
#define CURRENT_TIME			(startup_time + (jiffies / HZ))

#define CLOCK_REALTIME			0
#define CLOCK_MONOTONIC		 	1
#define CLOCK_MONOTONIC_RAW	 	4
#define CLOCK_BOOTTIME			7

#define ITIMER_REAL			0
#define ITIMER_VIRTUAL			1
#define ITIMER_PROF			2

#define	rdtsc(low, high)		__asm__ __volatile__("rdtsc" : "=a" (low), "=d" (high))
#define	rdtscl(low)			__asm__ __volatile__("rdtsc" : "=a" (low) : : "edx")

/*
 * Kernel time value structure.
 */
struct kernel_timeval {
	time_t	tv_sec;			/* seconds */
	time_t	tv_nsec;		/* nano seconds */
};

/*
 * Time value structure.
 */
struct timeval {
	int64_t	tv_sec;			/* seconds */
	int64_t	tv_usec;		/* micro seconds */
};

/*
 * Time value structure.
 */
struct old_timeval {
	long	tv_sec;			/* seconds */
	long	tv_usec;		/* micro seconds */
};

/*
 * Time specifications.
 */
struct timespec {
	int64_t	tv_sec;			/* seconds */
	int64_t	tv_nsec;		/* nano seconds */
};

/*
 * Time specifications.
 */
struct old_timespec {
	long	tv_sec;			/* seconds */
	long	tv_nsec;		/* nano seconds */
};

/*
 * Timer value.
 */
struct itimerval {
	int32_t it_interval_sec;	/* timer interval in seconds */
	int32_t it_interval_usec;	/* timer interval in micro seconds */
	int32_t it_value_sec;		/* current value in seconds */
	int32_t it_value_usec;		/* current value in micro seconds */
};

time_t mktime(uint32_t year, uint32_t month, int32_t day, uint32_t hour, uint32_t min, uint32_t sec);

/*
 * Convert ms to jiffies.
 */
static inline time_t ms_to_jiffies(uint32_t ms)
{
	return (ms + (1000L / HZ) - 1) / (1000L / HZ);
}

/*
 * Convert timespec to kernel time value.
 */
static inline void timespec_to_kernel_timeval(const struct timespec *ts, struct kernel_timeval *tv)
{
	tv->tv_sec = ts->tv_sec;
	tv->tv_nsec = ts->tv_nsec;
}

/*
 * Convert old time value to kernel time value.
 */
static inline void old_timeval_to_kernel_timeval(const struct old_timeval *otv, struct kernel_timeval *tv)
{
	tv->tv_sec = otv->tv_sec;
	tv->tv_nsec = otv->tv_usec * 1000L;
}

/*
 * Convert kernel time value to jiffies.
 */
static inline time_t kernel_timeval_to_jiffies(const struct kernel_timeval *tv)
{
	time_t nsec = tv->tv_nsec;

	/* convert nano seconds to jiffies */
	nsec += 1000000000L / HZ - 1;
	nsec /= 1000000000L / HZ;

	return tv->tv_sec * HZ + nsec;
}

/*
 * Convert timespec to jiffies.
 */
static inline time_t timespec_to_jiffies(const struct timespec *ts)
{
	time_t nsec = ts->tv_nsec;

	/* convert nano seconds to jiffies */
	nsec += 1000000000L / HZ - 1;
	nsec /= 1000000000L / HZ;

	return ts->tv_sec * HZ + nsec;
}

/*
 * Convert jiffies to timespec.
 */
static inline void jiffies_to_timespec(time_t jiffies, struct timespec *ts)
{
	ts->tv_sec = jiffies / HZ;
	ts->tv_nsec = jiffies % HZ;
}

/*
 * Convert timespec to jiffies.
 */
static inline time_t old_timespec_to_jiffies(const struct old_timespec *ts)
{
	time_t nsec = ts->tv_nsec;

	/* convert nano seconds to jiffies */
	nsec += 1000000000L / HZ - 1;
	nsec /= 1000000000L / HZ;

	return ts->tv_sec * HZ + nsec;
}

/*
 * Convert jiffies to timespec.
 */
static inline void jiffies_to_old_timespec(time_t jiffies, struct old_timespec *ts)
{
	ts->tv_nsec = (jiffies % HZ) * (1000000000L / HZ);
	ts->tv_sec = jiffies / HZ;
}

#endif
