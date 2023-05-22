#ifndef _LIBUTILS_H_
#define _LIBUTILS_H_

#include <stdio.h>

#define OPT_HELP			1000

#define FLAG_CP_FORCE			(1 << 0)
#define FLAG_CP_VERBOSE			(1 << 1)
#define FLAG_CP_RECURSIVE		(1 << 2)
#define FLAG_CP_PRESERVE_ATTR		(1 << 3)
#define FLAG_CP_PRESERVE_MODE		(1 << 4)

int build_path(char *dirname, char *filename, char *buf, int bufsiz);
int copy(const char *src, const char *dst, int flags, int follow, int level);
char *human_size(off_t size, char *buf, size_t buflen);

#endif
