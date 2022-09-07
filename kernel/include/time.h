#ifndef _TIME_H_
#define _TIME_H_

#include <stddef.h>

extern uint32_t startup_time;
extern volatile uint32_t jiffies;

#define SH_DIV(NOM, DEN, LSH) ((((NOM) / (DEN)) << (LSH)) + ((((NOM) % (DEN)) << (LSH)) + (DEN) / 2) / (DEN))

#define HZ				100
#define CURRENT_TIME			(startup_time + (jiffies / HZ))

#define CLOCK_REALTIME			0
#define CLOCK_MONOTONIC		 	1

#define ITIMER_REAL			0
#define ITIMER_VIRTUAL			1
#define ITIMER_PROF			2

#define	rdtsc(low, high)		__asm__ __volatile__("rdtsc" : "=a" (low), "=d" (high))

/* time stamp counter (defined in drivers/pit.c) */
extern uint32_t last_tsc_low;
extern uint32_t tsc_quotient;
extern volatile uint32_t time_offset_us;

/*
 * Time value structure.
 */
struct timeval_t {
	long	tv_sec;			/* seconds */
	long	tv_usec;		/* micro seconds */
};

/*
 * Time specifications.
 */
struct timespec_t {
	time_t	tv_sec;			/* seconds */
	time_t	tv_nsec;		/* nano seconds */
};

/*
 * Timer value.
 */
struct itimerval_t {
	int32_t it_interval_sec;	/* timer interval in seconds */
	int32_t it_interval_usec;	/* timer interval in micro seconds */
	int32_t it_value_sec;		/* current value in seconds */
	int32_t it_value_usec;		/* current value in micro seconds */
};

time_t mktime(uint32_t year, uint32_t month, int32_t day, uint32_t hour, uint32_t min, uint32_t sec);

/*
 * Convert ms to jiffies.
 */
static inline uint32_t ms_to_jiffies(uint32_t ms)
{
	return (ms + (1000L / HZ) - 1) / (1000L / HZ);
}

/*
 * Convert timespec to jiffies.
 */
static inline uint32_t timespec_to_jiffies(const struct timespec_t *ts)
{
	uint64_t nsec = ts->tv_nsec;

	/* convert nano seconds to jiffies */
	nsec += SH_DIV(1000000000L, HZ, 8) - 1;
	nsec /= SH_DIV(1000000000L, HZ, 8);

	return ts->tv_sec * HZ + nsec;
}

/*
 * Convert jiffies to timespec.
 */
static inline void jiffies_to_timespec(uint32_t jiffies, struct timespec_t *ts)
{
	ts->tv_nsec = (jiffies % HZ) * SH_DIV(1000000000L, HZ, 8);
	ts->tv_sec = jiffies / HZ;
}

/*
 * Get time offset (based on time stamp counter).
 */
static inline uint32_t get_time_offset()
{
	register uint32_t eax, edx;

	/* read time stamp counter */
	rdtsc(eax, edx);

	/* relative to previous jiffy */
	eax -= last_tsc_low;

	/* get time offset */
	__asm__("mull %2"
		:"=a" (eax), "=d" (edx)
		:"rm" (tsc_quotient),
		"0" (eax));

	return edx;
}

#endif
