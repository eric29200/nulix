#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

#include "../x86/__syscall.h"

char *getcwd(char *buf, size_t size)
{
	long ret;

	char tmp[buf ? 1 : PATH_MAX];

	if (!buf) {
		buf = tmp;
		size = PATH_MAX;
	} else if (!size) {
		errno = EINVAL;
		return NULL;
	}

	ret = syscall2(SYS_getcwd, (long) buf, size);
	if (ret < 0)
		return NULL;

	if (ret == 0 || buf[0] != '/') {
		errno = ENOENT;
		return NULL;
	}

	return buf == tmp ? strdup(buf) : buf;
}