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

int __getgrent(FILE *fp, struct group *gr, char **line, size_t *size, char ***mem, size_t *nrmem, struct group **res)
{
	char *s, *mems;
	int ret = 0;
	ssize_t n;
	size_t i;

	for (;;) {
		/* get next line */
		n = getline(line, size, fp);
		if (n < 0) {
			ret = ferror(fp) ? errno : 0;
			free(*line);
			*line = NULL;
			gr = NULL;
			goto out;
		}

		/* end line */
		line[0][n - 1] = 0;

		/* get group name */
		s = line[0];
		gr->gr_name = s++;
		s = strchr(s, ':');
		if (!s)
			continue;

		/* get password */
		*s++ = 0;
		gr->gr_passwd = s;
		s = strchr(s, ':');
		if (!s)
			continue;

		/* get group id */
		*s++ = 0;
		gr->gr_gid = atou(&s);
		if (*s != ':')
			continue;

		/* get users list */
		*s++ = 0;
		mems = s;
	
		break;
	}

	/* get number of users */
	for (*nrmem = !!*s; *s; s++)
		if (*s == ',')
			++*nrmem;

	/* allocate users list */
	free(*mem);
	*mem = calloc(sizeof(char *), *nrmem + 1);
	if (!*mem) {
		ret = errno;
		free(*line);
		*line = NULL;
		gr = NULL;
		goto out;
	}

	/* get users list */
	if (*mems) {
		mem[0][0] = mems;
		for (s = mems, i = 0; *s; s++) {
			if (*s == ',') {
				*s++ = 0;
				mem[0][++i] = s;
			}
		}

		mem[0][++i] = 0;
	} else {
		mem[0][0] = 0;
	}

	gr->gr_mem = *mem;
out:
	*res = gr;
	if (ret)
		errno = ret;
	return ret;
}