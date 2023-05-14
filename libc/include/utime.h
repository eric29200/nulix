#ifndef _LIBC_UTIME_H_
#define _LIBC_UTIME_H_

#include <sys/types.h>

struct utimbuf {
	time_t		actime;		/* last access time */
	time_t		modtime;	/* last modification time */
};

int utime(const char *filename, const struct utimbuf *times);

#endif