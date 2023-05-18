#include <stdio.h>
#include <libgen.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

#include "libutils/opt.h"

/*
 * Remove suffix.
 */
static void remove_suffix(char *name, char *suffix)
{
	char *np, *sp;

	/* go to end */
	np = name + strlen(name);
	sp = suffix + strlen(suffix);

	while (np > name && sp > suffix)
		if (*--np != *--sp)
			return;

	if (np > name)
		*np = 0;
}

/*
 * Usage.
 */
static void usage(const char *name)
{
	fprintf(stderr, "Usage: %s path [suffix]\n", name);
	fprintf(stderr, "\t  , --help\t\tprint help and exit\n");
}
 
/* options */
struct option long_opts[] = {
	{ "help",	no_argument,	0,	OPT_HELP	},
	{ 0,		0,		0,	0		},
};

int main(int argc, char **argv)
{
	const char *name = argv[0];
	char *b;
	int c;

	/* get options */
	while ((c = getopt_long(argc, argv, "", long_opts, NULL)) != -1) {
		switch (c) {
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
	if (argc != 1 && argc != 2) {
		usage(name);
		exit(1);
	}

	/* get basename */
	b = basename(*argv);

	/* remove suffix */
	if (argc == 2)
		remove_suffix(b, argv[1]);

	/* print result */
	printf("%s\n", b);

	return 0;
}
