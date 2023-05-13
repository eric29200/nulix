#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
 
 /*
  * Remove a directory.
  */
static int rm_dir(const char *name, int parent)
{
	int ret = 0, first = 1;
	char *line;

	for (;;) {
		/* remove directory */
		ret = rmdir(name);
		if (ret)
			break;

		/* get next parent */
		line = strrchr(name, '/');
		if (!line || !parent)
			break;

		while (line > name && *line == '/')
			--line;

		line[1] = 0;
		first = 0;
	}
	
	return ret && first;
}

/*
 * Usage.
 */
static void usage(const char *name)
{
	fprintf(stderr, "Usage: %s directory [...]\n", name);
}

int main(int argc, char **argv)
{
	int parent = 0, ret = 0, i;
	size_t len;

	/* check arguments */
	if (argc < 2) {
		usage(argv[0]);
		exit(EXIT_FAILURE);
	}

	/* parent argument */
	if (argv[1][0] == '-' && argv[1][1] == 'p')
		parent = 1;

	/* remove directories */
	for (i = parent + 1; i < argc; i++) {
		/* check name */
		if (argv[i][0] == '-') {
			usage(argv[0]);
			exit(EXIT_FAILURE);
		}

		/* remove end slashs */
		for (len = strlen(argv[i]); argv[i][len - 1] == '/' && len > 1; len--)
			argv[i][len - 1] = 0;

		/* remove directory */
		if (rm_dir(argv[i], parent)) {
			fprintf(stderr, "%s: cannot remove directory\n", argv[i]);
			ret = EXIT_FAILURE;
		}
	}

	return ret;
}
