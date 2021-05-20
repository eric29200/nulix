#ifndef _SYSCALL_H_
#define _SYSCALL_H_

#include <stddef.h>
#include <fs/fs.h>
#include <ipc/signal.h>
#include <uio.h>
#include <resource.h>
#include <time.h>

#define SYSCALLS_NUM            (__NR_statx + 1)

#define __NR_exit               1
#define __NR_fork               2
#define __NR_read               3
#define __NR_write              4
#define __NR_open               5
#define __NR_close              6
#define __NR_waitpid            7
#define __NR_creat              8
#define __NR_link               9
#define __NR_unlink             10
#define __NR_execve             11
#define __NR_chdir              12
#define __NR_lseek              19
#define __NR_getpid             20
#define __NR_setuid             23
#define __NR_getuid             24
#define __NR_access             33
#define __NR_kill               37
#define __NR_mkdir              39
#define __NR_rmdir              40
#define __NR_dup                41
#define __NR_brk                45
#define __NR_setgid             46
#define __NR_getgid             47
#define __NR_geteuid            49
#define __NR_getegid            50
#define __NR_ioctl              54
#define __NR_fcntl              55
#define __NR_setpgid            57
#define __NR_umask              60
#define __NR_dup2               63
#define __NR_getppid            64
#define __NR_sigaction          67
#define __NR_symlink            83
#define __NR_readlink           85
#define __NR_mmap               90
#define __NR_munmap             91
#define __NR_stat               106
#define __NR_wait4              114
#define __NR_sigreturn          119
#define __NR_modify_ldt         123
#define __NR_sigprocmask        126
#define __NR_getpgid            132
#define __NR_getdents           141
#define __NR_readv              145
#define __NR_writev             146
#define __NR_nanosleep          162
#define __NR_rt_sigaction       174
#define __NR_rt_sigprocmask     175
#define __NR_getcwd             183
#define __NR_mmap2              192
#define __NR_getuid32           199
#define __NR_getgid32           200
#define __NR_geteuid32          201
#define __NR_getegid32          202
#define __NR_setuid32           213
#define __NR_setgid32           214
#define __NR_getdents64         220
#define __NR_fcntl64            221
#define __NR_gettid             224
#define __NR_set_thread_area    243
#define __NR_get_thread_area    244
#define __NR_exit_group         252
#define __NR_set_tid_address    258
#define __NR_openat             295
#define __NR_mkdirat            296
#define __NR_unlinkat           301
#define __NR_faccessat          307
#define __NR_linkat             303
#define __NR_symlinkat          304
#define __NR_readlinkat         305
#define __NR_statx              383

typedef int32_t (*syscall_f)(uint32_t nr, ...);

void init_syscall();

void sys_exit(int status);
pid_t sys_fork();
int sys_read(int fd, char *buf, int count);
int sys_write(int fd, const char *buf, int count);
int sys_open(const char *pathname, int flags, mode_t mode);
int sys_close(int fd);
uint32_t sys_brk(uint32_t addr);
int sys_execve(const char *path, char *const argv[], char *const envp[]);
int sys_stat(const char *filename, struct stat_t *statbuf);
int sys_statx(int dirfd, const char *pathname, int flags, unsigned int mask, struct statx_t *statbuf);
int sys_dup(int oldfd);
int sys_dup2(int oldfd, int newfd);
pid_t sys_waitpid(pid_t pid, int *wstatus, int options);
pid_t sys_wait4(pid_t pid, int *wstatus, int options, struct rusage_t *rusage);
int sys_access(const char *filename, mode_t mode);
int sys_faccessat(int dirfd, const char *pathname, int flags);
int sys_chdir(const char *path);
int sys_mkdir(const char *pathname, mode_t mode);
off_t sys_lseek(int fd, off_t offset, int whence);
int sys_creat(const char *pathname, mode_t mode);
int sys_link(const char *oldpath, const char *newpath);
int sys_unlink(const char *pathname);
int sys_rmdir(const char *pathname);
int sys_getdents(int fd, struct dirent_t *dirent, uint32_t count);
ssize_t sys_readv(int fd, const struct iovec_t *iov, int iovcnt);
ssize_t sys_writev(int fd, const struct iovec_t *iov, int iovcnt);
int sys_nanosleep(const struct timespec_t *req, struct timespec_t *rem);
int sys_getdents64(int fd, void *dirp, size_t count);
pid_t sys_getpid();
pid_t sys_getppid();
pid_t sys_getpgid(pid_t pid);
int sys_setpgid(pid_t t, pid_t pgid);
int sys_sigaction(int signum, const struct sigaction_t *act, struct sigaction_t *oldact);
int sys_kill(pid_t pid, int sig);
int sys_sigprocmask(int how, const sigset_t *set, sigset_t *oldset);
int sys_rt_sigprocmask(int how, const sigset_t *set, sigset_t *oldset, size_t sigsetsize);
int sys_sigreturn();
uid_t sys_getuid();
uid_t sys_geteuid();
int sys_setuid(uid_t uid);
gid_t sys_getgid();
gid_t sys_getegid();
int sys_setgid(gid_t gid);
int sys_getcwd(char *buf, size_t size);
int sys_ioctl(int fd, unsigned long request, unsigned long arg);
int sys_fcntl(int fd, int cmd, unsigned long arg);
mode_t sys_umask(mode_t mask);
void *sys_mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset);
void *sys_mmap2(void *addr, size_t length, int prot, int flags, int fd, off_t pgoffset);
int sys_munmap(void *addr, size_t length);
int sys_modify_ldt(int func, void *ptr, unsigned long bytecount);
pid_t sys_gettid();
int sys_get_thread_area(void *u_info);
int sys_set_thread_area(void *u_info);
pid_t sys_set_tid_address(int *tidptr);
void sys_exit_group(int status);
int sys_mkdirat(int dirfd, const char *pathname, mode_t mode);
int sys_linkat(int olddirfd, const char *oldpath, int newdirfd, const char *newpath, int flags);
int sys_unlinkat(int dirfd, const char *pathname, int flags);
int sys_openat(int dirfd, const char *pathname, int flags, mode_t mode);
int sys_symlink(const char *target, const char *linkpath);
int sys_symlinkat(const char *target, int newdirfd, const char *linkpath);
ssize_t sys_readlink(const char *pathname, char *buf, size_t bufsize);
ssize_t sys_readlinkat(int dirfd, const char *pathname, char *buf, size_t bufsize);

#endif
