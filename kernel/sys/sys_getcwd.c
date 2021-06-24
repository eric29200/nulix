#include <sys/syscall.h>

/*
 * Get current working dir system call.
 */
int sys_getcwd(char *buf, size_t size)
{
  size_t n;

  /* check size limit */
  n = strlen(current_task->cwd_path) + 1;
  if (size < n)
    n = size;

  /* get current working dir path */
  strncpy(buf, current_task->cwd_path, n - 1);
  buf[n] = 0;

  return n;
}
