#include <sys/syscall.h>
#include <mm/mmap.h>

/*
 * Memory unmap system call.
 */
int sys_munmap(void *addr, size_t length)
{
	return do_munmap((uint32_t) addr, length);
}
