#include <sys/syscall.h>
#include <mm/mmap.h>

/*
 * Memory map system call.
 */
void *sys_mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset)
{
  return do_mmap(addr, length, prot, flags);
}
