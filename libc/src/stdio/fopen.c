#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include "__stdio_impl.h"
#include "../x86/__syscall.h"

FILE *fopen(const char *pathname, const char *mode)
{
	int flags, fd;
	FILE *fp;

	/* check mode */
	if (!strchr("rwa", *mode)) {
		errno = EINVAL;
		return NULL;
	}

	/* compute flags */
	flags = __fmodeflags(mode);

	/* open file */
	fd = open(pathname, flags, 0666);
	if (fd < 0)
		return NULL;

	/* close-on-exec */
	if (flags & O_CLOEXEC)
		__syscall3(SYS_fcntl, fd, F_SETFD, FD_CLOEXEC);

	/* create file */
	fp = fdopen(fd, mode);
	if (fp)
		return fp;

	/* on failure close file */
	__syscall1(SYS_close, fd);
	return NULL;
}
