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
#define SYS_link		9
#define SYS_unlink		10
#define SYS_execve		11
#define SYS_chdir		12
#define SYS_mknod		14
#define SYS_chmod		15
#define SYS_lseek		19
#define SYS_getpid		20
#define SYS_getuid		24
#define SYS_access		33
#define SYS_kill		37
#define SYS_rename		38
#define SYS_mkdir		39
#define SYS_rmdir		40
#define SYS_dup			41
#define SYS_brk			45
#define SYS_getgid		47
#define SYS_geteuid		49
#define SYS_getegid		50
#define SYS_ioctl		54
#define SYS_fcntl		55
#define SYS_umask		60
#define SYS_dup2		63
#define SYS_symlink		83
#define SYS_readlink		85
#define SYS_wait4		114
#define SYS_uname		122
#define SYS_readv		145
#define SYS_writev		146
#define SYS_nanosleep		162
#define SYS_getcwd		183
#define SYS_getdents64		220
#define SYS_fchownat		298
#define SYS_fstatat64		300
#define SYS_utimensat		320
#define SYS_clock_gettime64	403

void _exit(int status);
void _Exit(int status);
pid_t fork();
int execv(const char *pathname, char *const argv[]);
int execve(const char *pathname, char *const argv[], char *const envp[]);
int execvp(const char *file, char *const argv[]);
int execvpe(const char *file, char *const argv[], char *const envp[]);
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
int lchown(const char *pathname, uid_t owner, gid_t group);
int fchownat(int dirfd, const char *pathname, uid_t owner, gid_t group, int flags);
int access(const char *pathname, int mode);
int link(const char *oldpath, const char *newpath);
int symlink(const char *target, const char *linkpath);
int unlink(const char *pathname);
int rmdir(const char *pathname);
uid_t getuid();
uid_t geteuid();
gid_t getgid();
gid_t getegid();
ssize_t readlink(const char *pathname, char *buf, size_t bufsize);
char *getcwd(char *buf, size_t size);
int chdir(const char *path);
int gethostname(char *name, size_t len);
unsigned sleep(unsigned seconds);
pid_t getpid();

#endif
