#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <utime.h>
#include <getopt.h>

#include "../libutils/libutils.h"

/*
 * Usage.
 */
static void usage(const char *name)
{
	fprintf(stderr, "Usage: %s [-c] file [...]\n", name);
	fprintf(stderr, "\t-c, --no-create\t\tdo not create any files\n");
	fprintf(stderr, "\t  , --help\t\tprint help and exit\n");
}

/* options */
struct option long_opts[] = {
	{ "help",	no_argument,	0,	OPT_HELP	},
	{ "no-create",	no_argument,	0,	'c'		},
	{ 0,		0,		0,	0		},
};

int main(int argc, char **argv)
{
	int no_create = 0, fd, ret = 0, i, c;
	const char *name = argv[0];

	/* get options */
	while ((c = getopt_long(argc, argv, "c", long_opts, NULL)) != -1) {
		switch (c) {
			case 'c':
				no_create = 1;
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

	/* touch files */
	for (i = 0; i < argc; i++) {
		/* no create : change file date */
		if (no_create) {
			ret |= utime(argv[i], NULL);
			continue;
		}

		/* create file */
		fd = creat(argv[i], 0666);
		if (fd < 0) {
			fprintf(stderr, "%s: cannot create file\n", argv[i]);
			ret |= 1;
		} else {
			close(fd);
		}
	}

	return !!ret;
}
