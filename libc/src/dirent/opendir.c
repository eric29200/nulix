#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#include "../x86/__syscall.h"

DIR *opendir(const char *name)
{
	DIR *dir;
	int fd;

	/* open directory */
	fd = open(name, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
	if (fd < 0)
		return NULL;

	/* allocate directory */
	dir = calloc(1, sizeof(DIR));
	if (!dir) {
		__syscall1(SYS_close, fd);
		return NULL;
	}

	/* set directory */
	dir->fd = fd;

	return dir;
}
