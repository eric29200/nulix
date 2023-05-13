#include <string.h>
#include <errno.h>

#include "__passwd_impl.h"

int __getpw(const char *name, uid_t uid, struct passwd *pw, char **buf, size_t *size, struct passwd **res)
{
	int ret = 0;
	FILE *fp;

	/* reset result */
	*res = NULL;

	/* open passwd file */
	fp = fopen("/etc/passwd", "rbe");
	if (!fp) {
		ret = errno;
		goto out;
	}

	/* find entry */
	for (;;) {
		ret = __getpwent(fp, pw, buf, size, res);
		if (ret || !*res)
			break;

		if ((name && strcmp(name, (*res)->pw_name) == 0) || (!name && (*res)->pw_uid == uid))
			break;
	}

	/* close passwd file */
	fclose(fp);
 out:
	if (ret)
		errno = ret;

	return ret;
}