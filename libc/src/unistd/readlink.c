#include <unistd.h>

#include "../x86/__syscall.h"

ssize_t readlink(const char *pathname, char *buf, size_t bufsize)
{
	char dummy[1];
	int ret;

	if (!bufsize) {
		buf = dummy;
		bufsize = 1;
	}

	ret = __syscall3(SYS_readlink, (long) pathname, (long) buf, bufsize);

	if (buf == dummy && ret > 0)
		ret = 0;
	
	return __syscall_ret(ret);
}