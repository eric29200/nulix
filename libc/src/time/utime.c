#include <utime.h>
#include <time.h>
#include <fcntl.h>
#include <sys/stat.h>

int utime(const char *filename, const struct utimbuf *times)
{
	struct timespec ts[2] = {
		{ 
			.tv_sec		= times ? times->actime : 0,
			.tv_nsec	= 0,
		},
		{ 
			.tv_sec		= times ? times->modtime : 0,
			.tv_nsec	= 0,
		},
	};

	return utimensat(AT_FDCWD, filename, times ? ts : 0, 0);
}