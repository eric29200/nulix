#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <getopt.h>
#include <sys/stat.h>

#include "libutils/opt.h"

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
	fprintf(stderr, "\t  , --help\t\tprint help and exit\n");
	fprintf(stderr, "\t-p, --parents\t\tno error if existing, make parent directories as needed\n");
}

/* options */
struct option long_opts[] = {
	{ "help",	no_argument,	0,	OPT_HELP	},
	{ "parents",	no_argument,	0,	'p'		},
	{ 0,		0,		0,	0		},
};

int main(int argc, char **argv)
{
	const char *name = argv[0];
	int parent = 0, i, c;
	mode_t mode;
	int ret = 0;
	size_t len;

	/* get options */
	while ((c = getopt_long(argc, argv, "p", long_opts, NULL)) != -1) {
		switch (c) {
			case 'p':
				parent = 1;
				break;
			case OPT_HELP:
				usage(name);
				exit(0);
				break;
			default:
				exit(1);
				break;
		}
	}

	/* skip options */
	argc -= optind;
	argv += optind;

	/* check arguments */
	if (!argc) {
		usage(name);
		exit(1);
	}

	/* get mode */
	mode = 0777 & ~umask(0);

	/* create all directories */
	for (i = 0; i < argc; i++) {
		/* check argument */
		if (argv[i][0] == '-') {
			usage(argv[0]);
			exit(1);
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
