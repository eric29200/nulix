#include <sys/syscall.h>
#include <stdio.h>
#include <stderr.h>

/*
 * Get time system call.
 */
int sys_clock_gettime64(clockid_t clockid, struct timespec_t *tp)
{
  switch (clockid) {
    case CLOCK_REALTIME:
      tp->tv_sec = CURRENT_TIME;
      tp->tv_nsec = 0;
      break;
    case CLOCK_MONOTONIC:
      tp->tv_sec = jiffies / HZ;
      tp->tv_nsec = 0;
      break;
    default:
      printf("clock_gettime64 not implement on clockid=%d\n", clockid);
      return -ENOSYS;
  }

  return 0;
}
