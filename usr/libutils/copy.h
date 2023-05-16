#ifndef _CMD_COPY_H_
#define _CMD_COPY_H_

#define FLAG_CP_FORCE			(1 << 0)
#define FLAG_CP_VERBOSE			(1 << 1)
#define FLAG_CP_RECURSIVE		(1 << 2)
#define FLAG_CP_PRESERVE_ATTR		(1 << 3)
#define FLAG_CP_PRESERVE_MODE		(1 << 4)

int copy(const char *src, const char *dst, int flags, int follow, int level);

#endif