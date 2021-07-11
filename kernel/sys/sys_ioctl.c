#include <sys/syscall.h>
#include <drivers/tty.h>
#include <fcntl.h>
#include <stderr.h>
#include <stdio.h>
#include <dev.h>

/*
 * Ioctl system call.
 */
int sys_ioctl(int fd, unsigned long request, unsigned long arg)
{
  return do_ioctl(fd, request, arg);
}
