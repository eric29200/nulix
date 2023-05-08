#include <unistd.h>

#include "../x86/__syscall.h"

void _exit(int status)
{
	__syscall1(SYS_exit, status);
	for (;;);
}