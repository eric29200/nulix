#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include "__passwd_impl.h"

static unsigned atou(char **s)
{
	unsigned x;

	for (x = 0; **s - '0' < 10; ++*s)
		x = 10 * x + (**s - '0');

	return x;
}

int __getpwent(FILE *fp, struct passwd *pw, char **line, size_t *size, struct passwd **res)
{
	int ret = 0;
	ssize_t n;
	char *s;

	for (;;) {
		/* get next line */
		n = getline(line, size, fp);
		if (n < 0) {
			ret = ferror(fp) ? errno : 0;
			free(*line);
			*line = NULL;
			pw = NULL;
			break;
		}

		/* end line */
		line[0][n - 1] = 0;

		/* get user name */
		s = line[0];
		pw->pw_name = s++;
		s = strchr(s, ':');
		if (!s)
			continue;

		/* get password */
		*s++ = 0;
		pw->pw_passwd = s;
		s = strchr(s, ':');
		if (!s)
			continue;

		/* get user id */
		*s++ = 0;
		pw->pw_uid = atou(&s);
		if (*s != ':')
			continue;

		/* get group id */
		*s++ = 0;
		pw->pw_gid = atou(&s);
		if (*s != ':')
			continue;

		/* get user description */
		*s++ = 0;
		pw->pw_gecos = s;
		s = strchr(s, ':');
		if (!s)
			continue;

		/* get home directory */
		*s++ = 0;
		pw->pw_dir = s;
		s = strchr(s, ':');
		if (!s)
			continue;

		/* get shell path */
		*s++ = 0;
		pw->pw_shell = s;

		break;
	}

	/* set result */
	*res = pw;

	/* set errno */
	if (ret)
		errno = ret;

	return ret;
}