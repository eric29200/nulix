#include <sys/syscall.h>

/*
 * System info system call.
 */
int sys_sysinfo(struct sysinfo_t *info)
{
  memset(info, 0, sizeof(struct sysinfo_t));
  info->uptime = jiffies / HZ;
  info->totalram = 0;
  return 0;
}
