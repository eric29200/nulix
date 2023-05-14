#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

/*
 * Usage.
 */
static void usage(const char *name)
{
	fprintf(stderr, "Usage: %s fifo_name [...]\n", name);
}

int main(int argc, char **argv)
{
	int ret = 0, i;
	mode_t mode;

	/* check arguments */
	if (argc < 2) {
		usage(argv[0]);
		exit(1);
	}

	/* get mode */
	mode = 0666 & ~umask(0);

	for (i = 1; i < argc; i++) {
		if (mknod(argv[i], mode | S_IFIFO, 0) < 0) {
			ret = 1;
		}
	}

	return ret;
}
