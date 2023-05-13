#ifndef _LIBC_UNISTD_H_
#define _LIBC_UNISTD_H_

#include <stdio.h>
#include <stdint.h>

extern char **environ;

#define STDIN_FILENO	0
#define STDOUT_FILENO	1
#define STDERR_FILENO	2

#define SYS_exit		1
#define SYS_fork		2
#define SYS_read		3
#define SYS_write		4
#define SYS_open		5
#define SYS_close		6
#define SYS_chmod		15
#define SYS_lseek		19
#define SYS_mkdir		39
#define SYS_dup			41
#define SYS_brk			45
#define SYS_ioctl		54
#define SYS_fcntl		55
#define SYS_umask		60
#define SYS_dup2		63
#define SYS_readv		145
#define SYS_writev		146
#define SYS_getdents64		220
#define SYS_fchownat		298
#define SYS_fstatat64		300
#define SYS_clock_gettime64	403

void _exit(int status);
void _Exit(int status);
pid_t fork();
ssize_t read(int fd, void *buf, size_t count);
ssize_t write(int fd, const void *buf, size_t count);
int close(int fd);
int brk(void *addr);
void *sbrk(intptr_t increment);
off_t lseek(int fd, off_t offset, int whence);
int isatty(int fd);
int dup(int oldfd);
int dup2(int oldfd, int newfd);
int chown(const char *pathname, uid_t owner, gid_t group);
int fchownat(int dirfd, const char *pathname, uid_t owner, gid_t group, int flags);

#endif
