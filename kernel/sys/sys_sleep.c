#include <sys/syscall.h>
#include <delay.h>

/*
 * Sleep system call.
 */
int sys_sleep(unsigned long sec)
{
  msleep(sec * 1000);
  return 0;
}
