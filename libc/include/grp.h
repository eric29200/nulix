#ifndef _LIBC_GRP_H_
#define _LIBC_GRP_H_

#include <stdio.h>

struct group {
	char *		gr_name;	/* group name */
	char *		gr_passwd;	/* password */
	gid_t 		gr_gid;		/* group id */
	char **		gr_mem;		/* users list */
};

struct group *getgrgid(gid_t gid);
struct group *getgrnam(const char *name);

#endif