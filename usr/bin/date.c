#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <getopt.h>

#include "../libutils/libutils.h"

/*
 * Usage.
 */
static void usage(const char *name)
{
	fprintf(stderr, "Usage: %s\n", name);
	fprintf(stderr, "\t  , --help\t\tprint help and exit\n");
}

/* options */
struct option long_opts[] = {
	{ "help",	no_argument,	0,	OPT_HELP	},
	{ 0,		0,		0,	0		},
};

int main(int argc, char **argv)
{
	char *format = "%a %b %e %H:%M:%S %Z %Y";
	const char *name = argv[0];
	char buf[BUFSIZ];
	struct tm *tm;
	time_t t;
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
	if (argc) {
		usage(name);
		exit(1);
	}

	/* get current time */
	t = time(NULL);
	if (t == 1) {
		perror(name);
		exit(1);
	}

	/* get localtime */
	tm = localtime(&t);
	if (!tm) {
		perror(name);
		exit(1);
	}

	/* print format */
	strftime(buf, BUFSIZ, format, tm);
	printf("%s\n", buf);

	return 0;
}
