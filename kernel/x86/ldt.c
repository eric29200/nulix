#include <stdio.h>

/*
 * Modify ldt entry system call.
 */
int sys_modify_ldt(int func, void *ptr, unsigned long bytecount)
{
	UNUSED(func);
	UNUSED(ptr);
	UNUSED(bytecount);
	return 0;
}