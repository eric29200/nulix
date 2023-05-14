#include <sys/utsname.h>
#include <unistd.h>

#include "../x86/__syscall.h"

int uname(struct utsname *buf)
{
	return syscall1(SYS_uname, (long) buf);
}