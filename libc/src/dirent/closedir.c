#include <dirent.h>
#include <stdlib.h>
#include <unistd.h>

int closedir(DIR *dirp)
{
	int ret = close(dirp->fd);
	free(dirp);
	return ret;
}
