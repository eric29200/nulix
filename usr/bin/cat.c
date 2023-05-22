#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <getopt.h>

#include "../libutils/libutils.h"

static char buf[BUFSIZ];

/*
 * Print a file.
 */
static int print_fd(int fd)
{
	int n;

	while ((n = read(fd, buf, BUFSIZ)) > 0)
		if (write(STDOUT_FILENO, buf, n) != n)
			return 1;

	return n < 0 ? 1 : 0;
}

/*
 * Usage.
 */
static void usage(const char *name)
{
	fprintf(stderr, "Usage: %s [file ...]\n", name);
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
	int ret, fd, i, c;

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

	/* no argument : cat stdin */
	if (!argc) {
		ret = print_fd(STDIN_FILENO);
		if (ret)
			perror("stdin");

		return ret;
	}

	for (i = 0; i < argc; i++) {
		/* open file */
		if (argv[i][0] == '-' && argv[i][1] == '\0') {
			fd = STDIN_FILENO;
		} else if ((fd = open(argv[i], O_RDONLY)) < 0) {
			perror(argv[i]);
			return 1;
		}

		/* print file */
		ret = print_fd(fd);
		if (ret) {
			perror(argv[i]);
			close(fd);
			return ret;
		}

		/* close file */
		if (fd != STDIN_FILENO)
			close(fd);
	}

	return 0;
}
