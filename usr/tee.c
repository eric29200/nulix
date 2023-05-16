#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>

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
}

int main(int argc, char **argv)
{
	mode_t mode = O_WRONLY | O_CREAT;
	const char *name = argv[0];
	char buf[BUFSIZ], *p;
	int ret = 0, *fds;
	size_t nfds, i;
	ssize_t n;

	/* skip program name */
	argc--;
	argv++;

	/* parse arguments */
	while (*argv && argv[0][0] == '-') {
		for (p = &argv[0][1]; *p; p++) {
			switch(*p) {
				case 'a':
					mode |= O_APPEND;
					break;
				default:
					usage(name);
					exit(1);
			}
		}

		argc--;
		argv++;
	}

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
