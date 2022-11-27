#include <sys/syscall.h>

/*
 * Memory region remap system call.
 */
void *sys_mremap(void *old_address, size_t old_size, size_t new_size, int flags, void *new_address)
{
	return do_mremap((uint32_t) old_address, old_size, new_size, flags, (uint32_t) new_address);
}
