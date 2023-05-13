#ifndef _LIBC_PWD_H_
#define _LIBC_PWD_H_

#include <stdio.h>

struct passwd {
	char *		pw_name;	/* user name */
	char *		pw_passwd;	/* password */
	uid_t 		pw_uid;		/* user id */
	gid_t 		pw_gid;		/* group id */
	char *		pw_gecos;	/* description */
	char *		pw_dir;		/* home directory */
	char *		pw_shell;	/* shell path */
};

struct passwd *getpwuid(uid_t uid);
struct passwd *getpwnam(const char *name);

#endif