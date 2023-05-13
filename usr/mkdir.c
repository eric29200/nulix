#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <sys/stat.h>

/*
 * Make a directory.
 */
static int make_dir(const char *name, mode_t mode, int parent)
{
	char iname[PATH_MAX];
	char *line;

	/* copy name */
	strcpy(iname, name);

	/* parent : recurisve mkdir */
	line = strrchr(iname, '/');
	if (line && parent) {
		while (line > iname && *line == '/')
			line--;
		
		line[1] = 0;
		if (*line != '/')
			make_dir(iname, mode, 1);
	}

	/* make directory */
	if (mkdir(name, mode) < 0 && !parent)
		return 1;

	return 0;
}

/*
 * Usage.
 */
static void usage(const char *name)
{
	fprintf(stderr, "Usage: %s [-p] directory [...]\n", name);
}

int main(int argc, char **argv)
{
	int parent = 0, i;
	mode_t mode;
	int ret = 0;
	size_t len;

	/* check arguments */
	if (argc < 2) {
		usage(argv[0]);
		exit(EXIT_FAILURE);
	}

	/* parent option */
	if (argv[1][0] == '-' && argv[1][1] == 'p')
		parent = 1;

	/* get mode */
	mode = 0777 & ~umask(0);

	/* create all directories */
	for (i = parent + 1; i < argc; i++) {
		/* check argument */
		if (argv[i][0] == '-') {
			usage(argv[0]);
			exit(EXIT_FAILURE);
		}

		/* remove last '/' */
		len = strlen(argv[i]);
		if (argv[i][len - 1] == '/')
			argv[i][len - 1] = 0;

		/* make directory */
		ret = make_dir(argv[i], mode, parent);
		if (ret)
			fprintf(stderr, "%s: cannot create directory\n", argv[i]);
	}

	return ret;
}