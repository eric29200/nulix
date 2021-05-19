#include <sys/syscall.h>
#include <stderr.h>

/*
 * Set thread area system call.
 */
int sys_set_thread_area(void *u_info)
{
  UNUSED(u_info);
  return -ENOSYS;
}
