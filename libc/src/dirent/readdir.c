#include <dirent.h>
#include <errno.h>
#include <unistd.h>

#include "../x86/__syscall.h"

struct dirent *readdir(DIR *dirp)
{
	struct dirent *de;
	int len;

	/* read next entries */
	if (dirp->buf_pos >= dirp->buf_end) {
		len = __syscall3(SYS_getdents64, dirp->fd, (long) dirp->buf, sizeof(dirp->buf));
		if (len <= 0) {
			if (len < 0 && len != -ENOENT)
				errno = -len;

			return NULL;
		}

		dirp->buf_end = len;
		dirp->buf_pos = 0;
	}

	/* get next entry */
	de = (void *) (dirp->buf + dirp->buf_pos);

	/* update position */
	dirp->buf_pos += de->d_reclen;
	dirp->tell = de->d_off;

	return de;
}
