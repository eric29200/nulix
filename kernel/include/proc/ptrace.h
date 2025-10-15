#ifndef _PTRACE_H_
#define _PTRACE_H_

#include <stddef.h>

#define PTRACE_TRACEME		0
#define PTRACE_PEEKUSR		3
#define PTRACE_CONT		7
#define PTRACE_KILL		8
#define PTRACE_GETREGS		12
#define PTRACE_SYSCALL		24

#define PT_PTRACED		0x00000001
#define PT_TRACESYS		0x00000002

int sys_ptrace(long request, pid_t pid, uint32_t addr, uint32_t data);
void syscall_trace();

#endif