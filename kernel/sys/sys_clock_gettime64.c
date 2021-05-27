#include <sys/syscall.h>
#include <stdio.h>
#include <stderr.h>

/*
 * Get time system call.
 */
int sys_clock_gettime64(clockid_t clockid, struct timespec_t *tp)
{
  if (clockid != CLOCK_REALTIME) {
    printf("clock_gettime64 not implement on clockid=%d\n", clockid);
    return -ENOSYS;
  }

  tp->tv_sec = CURRENT_TIME;
  tp->tv_nsec = 0;    /* not implemented */

  return 0;
}
