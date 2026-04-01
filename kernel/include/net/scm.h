#ifndef _SCM_H_
#define _SCM_H_

#include <stddef.h>

/*
 * Credentials.
 */
struct ucred {
	pid_t			pid;
	uid_t			uid;
	gid_t			gid;
};

/*
 * Cookie.
 */
struct scm_cookie {
	struct ucred		creds;
};

int scm_send(struct scm_cookie *scm);

#endif