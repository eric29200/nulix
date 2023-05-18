#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <sys/stat.h>

#include "libutils/utils.h"

/*
 * Usage.
 */
static void usage(const char *name)
{
	fprintf(stderr, "Usage: %s fifo_name [...]\n", name);
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
	int ret = 0, i, c;
	mode_t mode;

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
	if (argc < 1) {
		usage(name);
		exit(1);
	}

	/* get mode */
	mode = 0666 & ~umask(0);

	for (i = 0; i < argc; i++) {
		if (mknod(argv[i], mode | S_IFIFO, 0) < 0) {
			ret = 1;
		}
	}

	return ret;
}
