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
      tp->tv_sec = CURRENT_TIME + (time_offset_us / 1000000L);
      tp->tv_nsec = (time_offset_us % 1000000L) * 1000L;
      break;
    case CLOCK_MONOTONIC:
      tp->tv_sec = (jiffies / HZ) + (time_offset_us / 1000000L);
      tp->tv_nsec = (time_offset_us % 1000000L) * 1000L;
      break;
    default:
      printf("clock_gettime64 not implement on clockid=%d\n", clockid);
      return -ENOSYS;
  }

  return 0;
}
