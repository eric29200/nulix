#ifndef _LIBC_TIME_IMPL_H_
#define _LIBC_TIME_IMPL_H_

#include <time.h>

time_t __tm_to_secs(const struct tm *tm);
int __month_to_secs(int month, int is_leap);
time_t __year_to_secs(time_t year, int *is_leap);
int __secs_to_tm(time_t t, struct tm *tm);

#endif