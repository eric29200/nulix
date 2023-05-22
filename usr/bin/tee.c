#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <getopt.h>
#include <fcntl.h>

#include "../libutils/libutils.h"

/*
 * Write a buffer to a file.
 */
static int write_full(int fd, char *buf, size_t len)
{
	int n;

	while (len > 0) {
		n = write(fd, buf, len);
		if (n < 0)
			return n;

		buf += n;
		len -= n;
	}

	return 0;
}

/*
 * Usage.
 */
static void usage(const char *name)
{
	fprintf(stderr, "Usage: %s [-a] [file ...]\n", name);
	fprintf(stderr, "\t-a, --append\t\tappend to files\n");
	fprintf(stderr, "\t  , --help\t\tprint help and exit\n");
}

/* options */
struct option long_opts[] = {
	{ "help",	no_argument,	0,	OPT_HELP	},
	{ "append",	no_argument,	0,	'a'		},
	{ 0,		0,		0,	0		},
};

int main(int argc, char **argv)
{
	mode_t mode = O_WRONLY | O_CREAT;
	const char *name = argv[0];
	int ret = 0, *fds, c;
	char buf[BUFSIZ];
	size_t nfds, i;
	ssize_t n;

	/* get options */
	while ((c = getopt_long(argc, argv, "a", long_opts, NULL)) != -1) {
		switch (c) {
			case 'a':
				mode |= O_APPEND;
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

	/* allocate file descriptors array */
	nfds = argc + 1;
	fds = (int *) calloc(nfds, sizeof(int));
	if (!fds) {
		perror("calloc");
		return 1;
	}

	/* open output files */
	for (i = 0; i < nfds - 1; i++) {
		if ((fds[i] = open(argv[i], mode, 0666)) < 0) {
			perror(argv[i]);
			ret = 1;
		}
	}

	/* add stdout */
	fds[i] = STDOUT_FILENO;

	/* read from stdin and write to all files */
	while ((n = read(STDIN_FILENO, buf, BUFSIZ)) > 0) {
		for (i = 0; i < nfds; i++) {
			if (fds[i] >= 0 && write_full(fds[i], buf, n) < 0) {
				perror(i == nfds - 1 ? "stdin" : argv[i]);
				fds[i] = -1;
				ret = 1;
			}
		}
	}

	return ret;
}
