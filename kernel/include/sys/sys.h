#ifndef _SYS_H_
#define _SYS_H_

#include <stddef.h>
#include <time.h>
#include <resource.h>
#include <sys/sysinfo.h>
#include <sys/utsname.h>

int sys_clock_gettime64(clockid_t clockid, struct timespec *tp);
int sys_clock_gettime32(clockid_t clockid, struct old_timespec *tp);
int sys_time(int *tloc);
int sys_getrandom(void *buf, size_t buflen, unsigned int flags);
int sys_getrusage(int who, struct rusage *ru);
int sys_pause();
int sys_prlimit64(pid_t pid, int resource, struct rlimit64 *new_limit, struct rlimit64 *old_limit);
int sys_reboot(int magic1, int magic2, int cmd, void *arg);
int sys_sysinfo(struct sysinfo *info);
mode_t sys_umask(mode_t mask);
int sys_uname(struct utsname *buf);
int sys_sethostname(const char *name, size_t len);
int sys_getpriority(int which, int who);
int sys_setpriority(int which, int who, int niceval);
int sys_prctl(int option, uint32_t arg2, uint32_t arg3, uint32_t arg4, uint32_t arg5);

#endif
