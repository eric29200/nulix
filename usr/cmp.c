#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>

/*
 * Usage.
 */
static void usage(const char *name)
{
	fprintf(stderr, "Usage: %s file1 file2\n", name);
}

int main(int argc, char **argv)
{
	char buf1[BUFSIZ], buf2[BUFSIZ];
	int fd1 = -1, fd2 = -1;
	ssize_t n, n1, n2, pos;
	int ret, i;

	/* check arguments */
	if (argc < 3) {
		usage(argv[0]);
		return EINVAL;
	}

	/* open file1 */
	fd1 = open(argv[1], 0);
	if (fd1 < 0) {
		ret = errno;
		perror(argv[1]);
		goto err;
	}

	/* open file2 */
	fd2 = open(argv[2], 0);
	if (fd2 < 0) {
		ret = errno;
		perror(argv[2]);
		goto err;
	}

	for (pos = 0;;) {
		/* read file1 */
		n1 = read(fd1, buf1, BUFSIZ);
		if (n1 < 0) {
			perror(argv[1]);
			goto err;
		}

		/* read file2 */
		n2 = read(fd2, buf2, BUFSIZ);
		if (n2 < 0) {
			perror(argv[2]);
			goto err;
		}

		/* end of files */
		if (!n1 && !n2)
			break;

		/* compare buffers */
		if (n1 == n2 && memcmp(buf1, buf2, n1) == 0) {
			pos += n1;
			continue;
		}

		/* find difference */
		n = n1 < n2 ? n1 : n2;
		for (i = 0; i < n; i++, pos++)
			if (buf1[i] != buf2[i])
				break;

		fprintf(stderr, "%s %s are different: byte %d\n", argv[0], argv[1], pos);
		ret = 1;
		goto err;
	}

	ret = 0;
err:
	if (fd1 > 0)
		close(fd1);
	if (fd2 > 0)
		close(fd2);
	return ret;
}