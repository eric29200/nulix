#ifndef _SYSCALL_H_
#define _SYSCALL_H_

#include <fs/fs.h>

#define SYSCALLS_NUM      (__NR_sbrk + 1)

#define __NR_exit         1
#define __NR_fork         2
#define __NR_read         3
#define __NR_write        4
#define __NR_mknod        5
#define __NR_open         6
#define __NR_close        7
#define __NR_waitpid      8
#define __NR_creat        9
#define __NR_dup          10
#define __NR_dup2         11
#define __NR_execve       12
#define __NR_sbrk         13

typedef int32_t (*syscall_f)(uint32_t nr, ...);

void init_syscall();

#endif
