#ifndef _SYS_H_
#define _SYS_H_

#include <stddef.h>
#include <time.h>
#include <resource.h>
#include <sys/sysinfo.h>
#include <sys/utsname.h>

int sys_clock_gettime64(clockid_t clockid, struct timespec_t *tp);
int sys_getrandom(void *buf, size_t buflen, unsigned int flags);
int sys_getrusage(int who, struct rusage_t *ru);
int sys_nanosleep(const struct old_timespec_t *req, struct old_timespec_t *rem);
int sys_pause();
int sys_prlimit64(pid_t pid, int resource, struct rlimit64_t *new_limit, struct rlimit64_t *old_limit);
int sys_reboot(int magic1, int magic2, int cmd, void *arg);
int sys_setitimer(int which, const struct itimerval_t *new_value, struct itimerval_t *old_value);
int sys_sysinfo(struct sysinfo_t *info);
mode_t sys_umask(mode_t mask);
int sys_uname(struct utsname_t *buf);

#endif