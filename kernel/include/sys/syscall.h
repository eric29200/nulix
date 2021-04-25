#ifndef _SYSCALL_H_
#define _SYSCALL_H_

#include <fs/fs.h>

#define SYSCALLS_NUM    (__NR_close + 1)

#define __NR_exit       1
#define __NR_fork       2
#define __NR_read       3
#define __NR_write      4
#define __NR_mknod      5
#define __NR_open       6
#define __NR_close      7

typedef int32_t (*syscall_f)(uint32_t nr, ...);

void init_syscall();

#endif
