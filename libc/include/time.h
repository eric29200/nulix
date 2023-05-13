#ifndef _LIBC_TIME_H_
#define _LIBC_TIME_H_

#include <stdio.h>

#define CLOCK_REALTIME		0

struct timespec {
	time_t			tv_sec;
	long			tv_nsec;
};

time_t time(time_t *tloc);
int clock_gettime(clockid_t clockid, struct timespec *tp);

#endif