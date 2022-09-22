#ifndef _SYSCALL_H_
#define _SYSCALL_H_

#include <fs/fs.h>
#include <ipc/signal.h>
#include <sys/utsname.h>
#include <sys/sysinfo.h>
#include <net/socket.h>
#include <proc/sched.h>
#include <time.h>
#include <resource.h>
#include <uio.h>
#include <stddef.h>

#define SYSCALLS_NUM			(__NR_faccessat2 + 1)

#define __NR_exit			1
#define __NR_fork			2
#define __NR_read			3
#define __NR_write			4
#define __NR_open			5
#define __NR_close			6
#define __NR_waitpid			7
#define __NR_creat			8
#define __NR_link			9
#define __NR_unlink			10
#define __NR_execve			11
#define __NR_chdir			12
#define __NR_mknod			14
#define __NR_chmod			15
#define __NR_lseek			19
#define __NR_getpid			20
#define __NR_mount			21
#define __NR_setuid			23
#define __NR_getuid			24
#define __NR_access			33
#define __NR_sync			36
#define __NR_kill			37
#define __NR_rename			38
#define __NR_mkdir			39
#define __NR_rmdir			40
#define __NR_dup			41
#define __NR_pipe			42
#define __NR_brk			45
#define __NR_setgid			46
#define __NR_getgid			47
#define __NR_geteuid			49
#define __NR_getegid			50
#define __NR_ioctl			54
#define __NR_fcntl			55
#define __NR_setpgid			57
#define __NR_umask			60
#define __NR_chroot			61
#define __NR_dup2			63
#define __NR_getppid			64
#define __NR_sigaction			67
#define __NR_symlink			83
#define __NR_readlink			85
#define __NR_reboot			88
#define __NR_mmap			90
#define __NR_munmap			91
#define __NR_fchmod			94
#define __NR_setitimer			104
#define __NR_wait4			114
#define __NR_sysinfo			116
#define __NR_fsync			118
#define __NR_sigreturn			119
#define __NR_clone			120
#define __NR_uname			122
#define __NR_modify_ldt			123
#define __NR_sigprocmask		126
#define __NR_getpgid			132
#define __NR_fchdir			133
#define __NR_llseek			140
#define __NR_select			142
#define __NR_readv			145
#define __NR_writev			146
#define __NR_nanosleep			162
#define __NR_poll			168
#define __NR_rt_sigaction		174
#define __NR_rt_sigprocmask		175
#define __NR_getcwd			183
#define __NR_vfork			190
#define __NR_mmap2			192
#define __NR_truncate64			193
#define __NR_ftruncate64		194
#define __NR_stat64			195
#define __NR_lstat64			196
#define __NR_fstat64			197
#define __NR_getuid32			199
#define __NR_getgid32			200
#define __NR_geteuid32			201
#define __NR_getegid32			202
#define __NR_setgroups32		206
#define __NR_fchown32			207
#define __NR_chown32			212
#define __NR_setuid32			213
#define __NR_setgid32			214
#define __NR_madvise			219
#define __NR_getdents64			220
#define __NR_fcntl64			221
#define __NR_gettid			224
#define __NR_set_thread_area		243
#define __NR_get_thread_area		244
#define __NR_exit_group			252
#define __NR_set_tid_address		258
#define __NR_statfs64			268
#define __NR_fstatfs64			269
#define __NR_openat			295
#define __NR_mkdirat			296
#define __NR_fchownat			298
#define __NR_fstatat64			300
#define __NR_unlinkat			301
#define __NR_fchmodat			306
#define __NR_faccessat			307
#define __NR_renameat			302
#define __NR_linkat			303
#define __NR_symlinkat			304
#define __NR_readlinkat			305
#define __NR_pselect6			308
#define __NR_utimensat			320
#define __NR_pipe2			331
#define __NR_renameat2			353
#define __NR_socket			359
#define __NR_bind			361
#define __NR_connect			362
#define __NR_listen			363
#define __NR_accept			364
#define __NR_getsockname		367
#define __NR_getpeername		368
#define __NR_sendto			369
#define __NR_recvfrom			371
#define __NR_recvmsg			372
#define __NR_copy_file_range		377
#define __NR_statx			383
#define __NR_clock_gettime64		403
#define __NR_faccessat2			439

typedef int32_t (*syscall_f)(uint32_t nr, ...);

void init_syscall();

void sys_exit(int status);
pid_t sys_fork();
pid_t sys_vfork();
int sys_read(int fd, char *buf, int count);
int sys_write(int fd, const char *buf, int count);
int sys_open(const char *pathname, int flags, mode_t mode);
int sys_close(int fd);
uint32_t sys_brk(uint32_t addr);
int sys_execve(const char *path, char *const argv[], char *const envp[]);
int sys_statx(int dirfd, const char *pathname, int flags, unsigned int mask, struct statx_t *statbuf);
int sys_dup(int oldfd);
int sys_dup2(int oldfd, int newfd);
pid_t sys_waitpid(pid_t pid, int *wstatus, int options);
pid_t sys_wait4(pid_t pid, int *wstatus, int options, struct rusage_t *rusage);
int sys_access(const char *filename, mode_t mode);
int sys_faccessat(int dirfd, const char *pathname, int flags);
int sys_faccessat2(int dirfd, const char *pathname, int flags);
int sys_chdir(const char *path);
int sys_mkdir(const char *pathname, mode_t mode);
off_t sys_lseek(int fd, off_t offset, int whence);
int sys_creat(const char *pathname, mode_t mode);
int sys_link(const char *oldpath, const char *newpath);
int sys_unlink(const char *pathname);
int sys_rmdir(const char *pathname);
ssize_t sys_readv(int fd, const struct iovec_t *iov, int iovcnt);
ssize_t sys_writev(int fd, const struct iovec_t *iov, int iovcnt);
int sys_nanosleep(const struct old_timespec_t *req, struct old_timespec_t *rem);
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
int sys_uname(struct utsname_t *buf);
int sys_pipe(int pipefd[2]);
int sys_pipe2(int pipefd[2], int flags);
int sys_clock_gettime64(clockid_t clockid, struct timespec_t *tp);
int sys_sysinfo(struct sysinfo_t *info);
int sys_rename(const char *oldpath, const char *newpath);
int sys_renameat(int olddirfd, const char *oldpath, int newdirfd, const char *newpath);
int sys_renameat2(int olddirfd, const char *oldpath, int newdirfd, const char *newpath, unsigned int flags);
int sys_socket(int domain, int type, int protocol);
int sys_sendto(int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, size_t addrlen);
int sys_recvfrom(int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, size_t addrlen);
int sys_recvmsg(int sockfd, struct msghdr_t *msg, int flags);
int sys_poll(struct pollfd_t *fds, size_t nfds, int timeout);
int sys_bind(int sockfd, const struct sockaddr *addr, size_t addrlen);
int sys_setitimer(int which, const struct itimerval_t *new_value, struct itimerval_t *old_value);
int sys_connect(int sockfd, const struct sockaddr *addr, size_t addrlen);
int sys_llseek(int fd, uint32_t offset_high, uint32_t offset_low, off_t *result, int whence);
int sys_fchmod(int fd, mode_t mode);
int sys_chmod(const char *pathname, mode_t mode);
int sys_listen(int sockfd, int backlog);
int sys_select(int nfds, fd_set_t *readfds, fd_set_t *writefds, fd_set_t *exceptfds, struct old_timeval_t *timeout);
int sys_pselect6(int nfds, fd_set_t *readfds, fd_set_t *writefds, fd_set_t *exceptfds, struct timespec_t *timeout, sigset_t *sigmask);
int sys_accept(int sockfd, struct sockaddr *addr, size_t addrlen);
int sys_mknod(const char *pathname, mode_t mode, dev_t dev);
int sys_getpeername(int sockfd, struct sockaddr *addr, size_t *addrlen);
int sys_getsockname(int sockfd, struct sockaddr *addr, size_t *addrlen);
int sys_chown(const char *pathname, uid_t owner, gid_t group);
int sys_fchown(int fd, uid_t owner, gid_t group);
int sys_setgroups(size_t size, const gid_t *list);
int sys_truncate64(const char *path, off_t length);
int sys_ftruncate64(int fd, off_t length);
int sys_utimensat(int dirfd, const char *pathname, struct timespec_t *times, int flags);
int sys_sync();
int sys_chroot(const char *path);
int sys_reboot();
int sys_mount(char *dev_name, char *dir_name, char *type, unsigned long flags, void *data);
int sys_statfs64(const char *path, size_t size, struct statfs64_t *buf);
int sys_fstatfs64(int fd, struct statfs64_t *buf);
int sys_fchmodat(int dirfd, const char *pathname, mode_t mode, unsigned int flags);
int sys_fchownat(int dirfd, const char *pathname, uid_t owner, gid_t group, unsigned int flags);
int sys_copy_file_range(int fd_in, off_t *off_in, int fd_out, off_t *off_out, size_t len, unsigned int flags);
int sys_stat64(const char *pathname, struct stat64_t *statbuf);
int sys_lstat64(const char *pathname, struct stat64_t *statbuf);
int sys_fstat64(int fd, struct stat64_t *statbuf);
int sys_fstatat64(int dirfd, const char *pathname, struct stat64_t *statbuf, int flags);
int sys_fsync(int fd);
int sys_fchdir(int fd);
int sys_madvise(void *addr, size_t length, int advice);
int sys_clone(uint32_t flags, uint32_t newsp);

#endif
