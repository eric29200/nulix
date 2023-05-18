#include <stdio.h>
#include <libgen.h>
#include <stdlib.h>
#include <getopt.h>

#include "libutils/opt.h"

/*
 * Usage.
 */
static void usage(const char *name)
{
	fprintf(stderr, "Usage: %s path\n", name);
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
	char *d;
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
	if (argc != 1) {
		usage(name);
		exit(1);
	}

	/* get dirname */
	d = dirname(*argv);
	printf("%s\n", d);

	return 0;
}
