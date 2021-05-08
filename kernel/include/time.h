#ifndef _TIME_H_
#define _TIME_H_

#include <stddef.h>

/*
 * Time value structure.
 */
struct timeval_t {
  time_t tv_sec;              /* seconds */
  time_t tv_usec;             /* micro seconds */
};

time_t mktime(uint32_t year, uint32_t month, int32_t day, uint32_t hour, uint32_t min, uint32_t sec);

#endif
