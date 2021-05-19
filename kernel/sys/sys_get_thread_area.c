#include <sys/syscall.h>
#include <stderr.h>

/*
 * Get thread area system call.
 */
int sys_get_thread_area(void *u_info)
{
  UNUSED(u_info);
  return -ENOSYS;
}
