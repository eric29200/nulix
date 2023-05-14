#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <limits.h>

int execvpe(const char *file, char *const argv[], char *const envp[])
{
	size_t len_path, len_name;
	bool seen_access = false;
	char *path, *p, *np;

	/* check file */
	if (!file) {
		errno = ENOENT;
		return -1;
	}

	/* full path : execve */
	if (strchr(file, '/'))
		return execve(file, argv, envp);

	/* get path */
	path = getenv("PATH");
	if (!path)
		path = "/bin:/usr/bin:/usr/local/bin";

	/* get name length */
	len_name = strnlen(file, NAME_MAX + 1);
	if (len_name > NAME_MAX) {
		errno = ENAMETOOLONG;
		return -1;
	}

	/* get path length */
	len_path = strnlen(path, PATH_MAX - 1) + 1;

	/* try each path */
	for (p = path;; p = np) {
		char tmp[len_name + len_path + 1];

		/* get next path */
		np = strchrnul(p, ':');
		if ((size_t) (np - p) >= len_path) {
			if (*np++)
				break;
			continue;
		}

		/* concat path + file */
		memcpy(tmp, p, np - p);
		tmp[np - p] = '/';
		memcpy(tmp + (np - p) + (np > p), file, len_name + 1);

		/* try exec */
		execve(tmp, argv, envp);

		/* exec returned/failed */
		switch (errno) {
			case EACCES:
				seen_access = true;
			case ENOENT:
			case ENOTDIR:
				break;
			default:
				errno = ENOENT;
				return -1;
		}

		if (!*np++)
			break;
	}

	if (seen_access)
		errno = EACCES;
	
	return -1;
}