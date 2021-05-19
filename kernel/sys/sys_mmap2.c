#include <sys/syscall.h>
#include <mm/mmap.h>

/*
 * Memory map 2 system call.
 */
void *sys_mmap2(void *addr, size_t length, int prot, int flags, int fd, off_t pgoffset)
{
  return do_mmap(addr, length, prot, flags);
}
