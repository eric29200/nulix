#include <libgen.h>
#include <stdio.h>
#include <string.h>

char *dirname(char *path)
{
	size_t i;

	if (!path || !*path)
		return ".";

	/* remove end slashes */
	for (i = strlen(path) - 1; path[i] == '/'; i--)
		if (!i)
			return "/";

	/* remove last file/directory */
	for (; path[i] != '/'; i--)
		if (!i)
			return ".";

	/* remove multiple slashes */
	for (; path[i] == '/'; i--)
		if (!i)
			return "/";

	path[i + 1] = 0;
	return path;
}
