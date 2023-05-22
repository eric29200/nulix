#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <getopt.h>
#include <sys/stat.h>

#include "../libutils/libutils.h"

#define mkdev(major, minor)	((major) << 8 | (minor))

/*
 * Usage.
 */
static void usage(const char *name)
{
	fprintf(stderr, "Usage: %s device [bcup] major minor\n", name);
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
	int major, minor, c;
	mode_t mode;

	/* get options */
	while ((c = getopt_long(argc, argv, "", long_opts, NULL)) != -1) {
		switch (c) {
			case  OPT_HELP:
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

	/* set mode */
	mode = 0666 & ~umask(0);

	/* create a fifo */
	if (argc == 2 && argv[1][0] == 'p') {
		if (mknod(argv[0], mode | S_IFIFO, 0) < 0) {
			perror(argv[0]);
			exit(1);
		}

		return 0;
	}

	/* check arguments */
	if (argc != 4) {
		usage(name);
		exit(1);
	}

	/* get device type */
	switch (argv[1][0]) {
		case 'b':
		 	mode |= S_IFBLK;
			break;
		case 'c':
			mode |= S_IFCHR;
			break;
		case 'u':
			mode |= S_IFCHR;
			break;
		default:
			usage(name);
			exit(1);
	}

	/* get major/minor numbers */
	major = atoi(argv[2]);
	minor = atoi(argv[3]);

	/* create device */
	if (errno != ERANGE) {
		if (mknod(argv[0], mode, mkdev(major, minor)) < 0) {
			perror(argv[0]);
			exit(1);
		}
	}

	return 0;
}
