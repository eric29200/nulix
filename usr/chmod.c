#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <getopt.h>

#include "libutils/utils.h"

/*
 * Usage.
 */
static void usage(const char *name)
{
	fprintf(stderr, "Usage: %s mode file [...]\n", name);
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
	mode_t mode = 0;
	char *mode_str;
	int i, c;

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
	if (argc < 2) {
		usage(name);
		exit(1);
	}

	/* get mode */
	for (mode_str = argv[0]; *mode_str >= '0' && *mode_str < '8'; mode_str++)
		mode = mode * 8 + (*mode_str - '0');

	/* check mode */
	if (*mode_str) {
		fprintf(stderr, "Mode must be specified as an octal number\n");
		exit(1);
	}

	/* chmod all files */
	for (i = 1; i < argc; i++)
		if (chmod(argv[i], mode) < 0)
			perror(argv[i]);

	return 0;
}
