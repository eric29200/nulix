#ifndef _SYSCALL_H_
#define _SYSCALL_H_

#include <stddef.h>
#include <fs/fs.h>

#define SYSCALLS_NUM      (__NR_wait + 1)

#define __NR_exit         1
#define __NR_fork         2
#define __NR_read         3
#define __NR_write        4
#define __NR_open         5
#define __NR_close        6
#define __NR_stat         7
#define __NR_execve       8
#define __NR_sbrk         9
#define __NR_sleep        10
#define __NR_dup          11
#define __NR_dup2         12
#define __NR_wait         13

typedef int32_t (*syscall_f)(uint32_t nr, ...);

void init_syscall();

void sys_exit(int status);
pid_t sys_fork();
int sys_read(int fd, char *buf, int count);
int sys_write(int fd, const char *buf, int count);
int sys_open(const char *pathname);
int sys_close(int fd);
void *sys_sbrk(size_t n);
int sys_execve(const char *path, char *const argv[], char *const envp[]);
int sys_stat(char *filename, struct stat_t *statbuf);
int sys_sleep(unsigned long secs);
int sys_dup(int oldfd);
int sys_dup2(int oldfd, int newfd);
int sys_wait();

#endif
