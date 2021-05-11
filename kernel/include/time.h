#ifndef _TIME_H_
#define _TIME_H_

#include <stddef.h>
#include <proc/sched.h>

/*
 * Time value structure.
 */
struct timeval_t {
  time_t  tv_sec;              /* seconds */
  time_t  tv_usec;             /* micro seconds */
};

/*
 * Time specifications.
 */
struct timespec_t {
  time_t  tv_sec;              /* seconds */
  long    tv_nsec;               /* nano seconds */
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
  uint32_t nsec = ts->tv_nsec;

  /* convert nano seconds to jiffies */
  nsec += 1000000000L / HZ - 1;
  nsec /= 1000000000L / HZ;

  return ts->tv_sec * HZ + nsec;
}

/*
 * Convert jiffies to timespec.
 */
static inline void jiffies_to_timespec(uint32_t jiffies, struct timespec_t *ts)
{
  ts->tv_nsec = (jiffies % HZ) * (1000000000L / HZ);
  ts->tv_sec = jiffies / HZ;
}

#endif
