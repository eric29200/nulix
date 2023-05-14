#ifndef _LIBC_WAIT_H_
#define _LIBC_WAIT_H_

#include <stdio.h>

pid_t waitpid(pid_t pid, int *wstatus, int options);

#endif