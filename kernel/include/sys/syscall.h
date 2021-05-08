#ifndef _SYSCALL_H_
#define _SYSCALL_H_

#include <stddef.h>
#include <fs/fs.h>
#include <uio.h>
#include <resource.h>

#define SYSCALLS_NUM      (__NR_writev + 1)

#define __NR_exit         1
#define __NR_fork         2
#define __NR_read         3
#define __NR_write        4
#define __NR_open         5
#define __NR_close        6
#define __NR_waitpid      7
#define __NR_creat        8
#define __NR_unlink       10
#define __NR_execve       11
#define __NR_chdir        12
#define __NR_lseek        19
#define __NR_access       33
#define __NR_mkdir        39
#define __NR_rmdir        40
#define __NR_dup          41
#define __NR_sbrk         45
#define __NR_dup2         63
#define __NR_stat         106
#define __NR_wait4        114
#define __NR_getdents     141
#define __NR_readv        145
#define __NR_writev       146

typedef int32_t (*syscall_f)(uint32_t nr, ...);

void init_syscall();

void sys_exit(int status);
pid_t sys_fork();
int sys_read(int fd, char *buf, int count);
int sys_write(int fd, const char *buf, int count);
int sys_open(const char *pathname, int flags, mode_t mode);
int sys_close(int fd);
void *sys_sbrk(size_t n);
int sys_execve(const char *path, char *const argv[], char *const envp[]);
int sys_stat(const char *filename, struct stat_t *statbuf);
int sys_dup(int oldfd);
int sys_dup2(int oldfd, int newfd);
pid_t sys_waitpid(pid_t pid, int *wstatus, int options);
pid_t sys_wait4(int *wstatus, int options, struct rusage_t *rusage);
int sys_access(const char *filename);
int sys_chdir(const char *path);
int sys_mkdir(const char *pathname, mode_t mode);
off_t sys_lseek(int fd, off_t offset, int whence);
int sys_creat(const char *pathname, mode_t mode);
int sys_unlink(const char *pathname);
int sys_rmdir(const char *pathname);
int sys_getdents(int fd, struct dirent_t *dirent, uint32_t count);
ssize_t sys_readv(int fd, const struct iovec_t *iov, int iovcnt);
ssize_t sys_writev(int fd, const struct iovec_t *iov, int iovcnt);

#endif
