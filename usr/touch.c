#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <utime.h>

/*
 * Usage.
 */
static void usage(const char *name)
{
	fprintf(stderr, "Usage: %s [-c] file [...]\n", name);
}

int main(int argc, char **argv)
{
	int no_create = 0, fd, ret = 0, i;

	/* check arguments */
	if (argc < 2) {
		usage(argv[0]);
		exit(EXIT_FAILURE);
	}

	/* get argument */
	if (argv[1][0] == '-' && argv[1][1] == 'c')
		no_create = 1;

	/* touch files */
	for (i = no_create + 1; i < argc; i++) {
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