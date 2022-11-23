#include <sys/syscall.h>
#include <stderr.h>

/*
 * Uname system call.
 */
int sys_uname(struct utsname_t *buf)
{
	if (!buf)
		return -EINVAL;

	strncpy(buf->sysname, "nulix", UTSNAME_LEN);
	strncpy(buf->nodename, "nulix", UTSNAME_LEN);
	strncpy(buf->release, "0.0.1", UTSNAME_LEN);
	strncpy(buf->version, "nulix 0.0.1", UTSNAME_LEN);
	strncpy(buf->machine, "x86", UTSNAME_LEN);

	return 0;
}
