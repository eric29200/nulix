#include <string.h>
#include <errno.h>

#include "__passwd_impl.h"

int __getgr(const char *name, gid_t gid, struct group *gr, char **buf, size_t *size, char ***mem, size_t *nrmem, struct group **res)
{
	int ret = 0;
	FILE *fp;

	/* reset result */
	*res = NULL;

	/* open group file */
	fp = fopen("/etc/group", "rbe");
	if (!fp) {
		ret = errno;
		goto out;
	}

	/* find entry */
	for (;;) {
		ret = __getgrent(fp, gr, buf, size, mem, nrmem, res);
		if (ret || !*res)
			break;

		if ((name && strcmp(name, (*res)->gr_name) == 0) || (!name && (*res)->gr_gid == gid))
			break;
	}

	/* close group file */
	fclose(fp);
 out:
	if (ret)
		errno = ret;

	return ret;
}