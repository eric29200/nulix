#ifndef _UNISTD_H_
#define _UNISTD_H_

#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>

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
#define __NR_getdents     141

/*
 * System call with no argument.
 */
static inline int syscall0(int n)
{
  int ret;
  __asm__ __volatile__("int $0x80" : "=a" (ret) : "0" (n));
  return ret;
}

/*
 * System call with 1 argument.
 */
static inline int syscall1(int n, int p1)
{
  int ret;
  __asm__ __volatile__("int $0x80" : "=a" (ret) : "0" (n), "b" (p1));
  return ret;
}

/*
 * System call with 2 arguments.
 */
static inline int syscall2(int n, int p1, int p2)
{
  int ret;
  __asm__ __volatile__("int $0x80" : "=a" (ret) : "0" (n), "b" (p1), "c" (p2));
  return ret;
}

/*
 * System call with 3 arguments.
 */
static inline int syscall3(int n, int p1, int p2, int p3)
{
  int ret;
  __asm__ __volatile__("int $0x80" : "=a" (ret) : "0" (n), "b" (p1), "c" (p2), "d" (p3));
  return ret;
}

/*
 * Exit system call.
 */
static inline void exit(int status)
{
  syscall1(__NR_exit, status);
}

/*
 * Fork system call.
 */
static inline pid_t fork()
{
  return syscall0(__NR_fork);
}

/*
 * Read system call.
 */
static inline ssize_t read(int fd, void *buf, size_t count)
{
  return syscall3(__NR_read, fd, (int) buf, count);
}

/*
 * Write system call.
 */
static inline ssize_t write(int fd, void *buf, size_t count)
{
  return syscall3(__NR_write, fd, (int) buf, count);
}

/*
 * Open system call.
 */
static inline int open(const char *pathname, int flags, mode_t mode)
{
  return syscall3(__NR_open, (int) pathname, flags, mode);
}

/*
 * Close system call.
 */
static inline int close(int fd)
{
  return syscall1(__NR_close, fd);
}

/*
 * Stat system call.
 */
static inline int stat(const char *pathname, struct stat *statbuf)
{
  return syscall2(__NR_stat, (int) pathname, (int) statbuf);
}

/*
 * Execve system call.
 */
static inline int execve(const char *path, char *const argv[], char *const envp[])
{
  return syscall3(__NR_execve, (int) path, (int) argv, (int) envp);
}

/*
 * Sbrk system call.
 */
static inline void *sbrk(size_t n)
{
  return (void *) syscall1(__NR_sbrk, n);
}

/*
 * Dup system call.
 */
static inline int dup(int oldfd)
{
  return syscall1(__NR_dup, oldfd);
}

/*
 * Dup2 system call.
 */
static inline int dup2(int oldfd, int newfd)
{
  return syscall2(__NR_dup2, oldfd, newfd);
}

/*
 * Wait system call.
 */
static inline pid_t waitpid(pid_t pid, int *wstatus, int options)
{
  return syscall3(__NR_waitpid, pid, (int) wstatus, options);
}

/*
 * Access system call.
 */
static inline int access(const char *pathname)
{
  return syscall1(__NR_access, (int) pathname);
}

/*
 * Change directory system call.
 */
static inline int chdir(const char *path)
{
  return syscall1(__NR_chdir, (int) path);
}

/*
 * Mkdir system call.
 */
static inline int mkdir(const char *pathname, mode_t mode)
{
  return syscall2(__NR_mkdir, (int) pathname, mode);
}

/*
 * Lseek system call.
 */
static inline int lseek(int fd, off_t offset, int whence)
{
  return syscall3(__NR_lseek, fd, offset, whence);
}

/*
 * Creat system call.
 */
static inline int creat(const char *pathname, mode_t mode)
{
  return syscall2(__NR_creat, (int) pathname, mode);
}

/*
 * Unlink system call.
 */
static inline int unlink(const char *pathname)
{
  return syscall1(__NR_unlink, (int) pathname);
}

/*
 * Rmdir system call.
 */
static inline int rmdir(const char *pathname)
{
  return syscall1(__NR_rmdir, (int) pathname);
}

/*
 * Get directory entries system call.
 */
static inline int getdents(int fd, struct dirent *dirent, uint32_t count)
{
  return syscall3(__NR_getdents, fd, (int) dirent, count);
}

#endif
