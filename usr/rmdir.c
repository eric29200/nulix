#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <getopt.h>
#include <unistd.h>

#include "libutils/opt.h"
 
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
	fprintf(stderr, "Usage: %s [-p] directory [...]\n", name);
	fprintf(stderr, "\t-p, --parents\t\tremove directory and its parents\n");
	fprintf(stderr, "\t  , --help\t\tprint help and exit\n");
}

/* options */
struct option long_opts[] = {
	{ "help",	no_argument,	0,	OPT_HELP	},
	{ "parents",	no_argument,	0,	'p'		},
	{ 0,		0,		0,	0		},
};

int main(int argc, char **argv)
{
	int parent = 0, ret = 0, i, c;
	const char *name = argv[0];
	size_t len;

	/* get options */
	while ((c = getopt_long(argc, argv, "p", long_opts, NULL)) != -1) {
		switch (c) {
			case OPT_HELP:
				usage(name);
				exit(0);
				break;
			case 'p':
				parent = 1;
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

	/* remove directories */
	for (i = 0; i < argc; i++) {
		/* remove end slashs */
		for (len = strlen(argv[i]); argv[i][len - 1] == '/' && len > 1; len--)
			argv[i][len - 1] = 0;

		/* remove directory */
		if (rm_dir(argv[i], parent)) {
			fprintf(stderr, "%s: cannot remove directory\n", argv[i]);
			ret = 1;
		}
	}

	return ret;
}
