#include <sys/syscall.h>
#include <mm/mmap.h>
#include <stdio.h>

/*
 * Memory map system call.
 */
void *sys_mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset)
{
	if (fd >= 0 || offset > 0) {
		printf("Unknown mmap system call : fd = %d, prot = %x, offset = %d\n", fd, prot, offset);
		return NULL;
	}

	return do_mmap((uint32_t) addr, length, flags);
}
