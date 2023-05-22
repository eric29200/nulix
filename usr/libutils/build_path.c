#include <stdio.h>
#include <string.h>
#include <libgen.h>

#include "libutils.h"

/*
 * Build a path.
 */
int build_path(char *dirname, char *filename, char *buf, int bufsiz)
{
	size_t dirname_len = strlen(dirname);
	int len, i;

	/* remove end slashes in dirname */
	for (i = dirname_len - 1; i && dirname[i] == '/'; i--)
		dirname[i] = 0;

	/* concat dirname and filename */
	len = snprintf(buf, bufsiz, "%s/%s", dirname, basename(filename));

	/* filename too long */
	if (len < 0 || len >= bufsiz) {
		fprintf(stderr, "'%s%s': filename too long\n", dirname, basename(filename));
		return 1;
	}

	return 0;
}
