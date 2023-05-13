#include <libgen.h>
#include <stdio.h>
#include <string.h>

char *basename(char *path)
{
	size_t i;

	if (!path || !*path)
		return ".";

	/* remove end slashes */
	for (i = strlen(path) - 1; i && path[i] == '/'; i--)
		path[i] = 0;

	/* find last slash */
	for (; i && path[i - 1] != '/'; i--);

	return path + i;
}
