#include <sys/syscall.h>

/*
 * Exit group system call.
 */
void sys_exit_group(int status)
{
  sys_exit(status);
}
