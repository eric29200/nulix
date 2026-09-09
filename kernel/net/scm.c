#include <net/socket.h>
#include <net/sock.h>
#include <stdio.h>

/*
 * Prepare cookie.
 */
int scm_send(struct scm_cookie *scm)
{
	memset(scm, 0, sizeof(struct scm_cookie));
	scm->creds.uid = current->uid;
	scm->creds.gid = current->gid;
	scm->creds.pid = current->pid;
	return 0;
}
