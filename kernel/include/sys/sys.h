#ifndef _SYS_H_
#define _SYS_H_

#include <stddef.h>
#include <time.h>
#include <resource.h>
#include <sys/sysinfo.h>
#include <sys/utsname.h>

int sys_clock_gettime64(clockid_t clockid, struct timespec *tp);
int sys_clock_gettime32(clockid_t clockid, struct old_timespec *tp);
int sys_getrandom(void *buf, size_t buflen, unsigned int flags);
int sys_getrusage(int who, struct rusage *ru);
int sys_nanosleep(const struct old_timespec *req, struct old_timespec *rem);
int sys_pause();
int sys_prlimit64(pid_t pid, int resource, struct rlimit64 *new_limit, struct rlimit64 *old_limit);
int sys_reboot(int magic1, int magic2, int cmd, void *arg);
int sys_setitimer(int which, const struct itimerval *new_value, struct itimerval *old_value);
int sys_sysinfo(struct sysinfo *info);
mode_t sys_umask(mode_t mask);
int sys_uname(struct utsname *buf);
int sys_sethostname(const char *name, size_t len);
int sys_getpriority(int which, int who);
int sys_setpriority(int which, int who, int niceval);

#endif
