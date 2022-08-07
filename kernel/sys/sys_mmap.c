#include <sys/syscall.h>
#include <mm/mmap.h>
#include <stdio.h>

/*
 * Memory map system call.
 */
void *sys_mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset)
{
	struct file_t *filp = NULL;

	if (offset > 0) {
		printf("Unknown mmap system call : fd = %d, prot = %x, offset = %d\n", fd, prot, offset);
		return NULL;
	}

	/* get file */
	if (fd >= 0) {
		if (fd >= NR_OPEN || !current_task->filp[fd])
			return NULL;

		filp = current_task->filp[fd];
	}

	return do_mmap((uint32_t) addr, length, flags, filp);
}
