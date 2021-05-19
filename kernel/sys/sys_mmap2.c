#include <sys/syscall.h>
#include <mm/mmap.h>
#include <stdio.h>

/*
 * Memory map 2 system call.
 */
void *sys_mmap2(void *addr, size_t length, int prot, int flags, int fd, off_t pgoffset)
{
  if (fd >= 0 || pgoffset > 0) {
    printf("Unknown mmap2 system call : fd = %d, prot = %x, pgoffset = %d\n", fd, prot, pgoffset);
    return NULL;
  }

  return do_mmap((uint32_t) addr, length, flags);
}
