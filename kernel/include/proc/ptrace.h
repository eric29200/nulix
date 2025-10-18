#ifndef _PTRACE_H_
#define _PTRACE_H_

#include <stddef.h>

#define PTRACE_TRACEME		0
#define PTRACE_PEEKUSR		3
#define PTRACE_CONT		7
#define PTRACE_KILL		8
#define PTRACE_GETREGS		12
#define PTRACE_ATTACH		16
#define PTRACE_SYSCALL		24
#define PTRACE_SEIZE		0x4206
#define PTRACE_SETOPTIONS	0x4200

#define PT_OPT_FLAG_SHIFT	3
#define PTRACE_O_EXITKILL	(1 << 20)
#define PTRACE_O_MASK		(0x000000ff | PTRACE_O_EXITKILL)

#define PT_PTRACED		0x00000001
#define PT_TRACESYS		0x00000002

int sys_ptrace(long request, pid_t pid, uint32_t addr, uint32_t data);
void syscall_trace();

#endif