#include <sys/syscall.h>
#include <mm/mmap.h>
#include <stdio.h>

/*
 * Memory map 2 system call.
 */
void *sys_mmap2(void *addr, size_t length, int prot, int flags, int fd, off64_t pgoffset)
{
	struct file_t *filp = NULL;

	/* get file */
	if (fd >= 0) {
		if (fd >= NR_OPEN || !current_task->files->filp[fd])
			return NULL;

		filp = current_task->files->filp[fd];
	}

	return do_mmap((uint32_t) addr, length, prot, flags, filp, pgoffset * PAGE_SIZE);
}
