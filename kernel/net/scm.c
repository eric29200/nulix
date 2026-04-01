#include <net/socket.h>
#include <net/sock.h>
#include <stdio.h>

/*
 * Prepare cookie.
 */
int scm_send(struct scm_cookie *scm)
{
	memset(scm, 0, sizeof(struct scm_cookie));
	scm->creds.uid = current_task->uid;
	scm->creds.gid = current_task->gid;
	scm->creds.pid = current_task->pid;
	return 0;
}