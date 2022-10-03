#include <sys/syscall.h>

/*
 * Mprotect system call.
 */
int sys_mprotect(void *addr, size_t len, int prot)
{
	UNUSED(addr);
	UNUSED(len);
	UNUSED(prot);
	return 0;
}
