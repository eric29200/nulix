#include <unistd.h>

#include "../x86/__syscall.h"

int dup2(int oldfd, int newfd)
{
	int ret;

	for (;;) {
		ret = __syscall2(SYS_dup2, oldfd, newfd);
		if (ret != -EBUSY)
			break;
	}

	return __syscall_ret(ret);
}
