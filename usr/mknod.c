#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>

#define mkdev(major, minor)	((major) << 8 | (minor))

/*
 * Usage.
 */
static void usage(const char *name)
{
	fprintf(stderr, "Usage: %s device [bcup] major minor\n", name);
}

int main(int argc, char **argv)
{
	int major, minor;
	mode_t mode;

	/* set mode */
	mode = 0666 & ~umask(0);

	/* create a fifo */
	if (argc == 3 && argv[2][0] == 'p') {
		if (mknod(argv[1], mode | S_IFIFO, 0) < 0) {
			perror(argv[1]);
			exit(1);
		}

		return 0;
	}

	/* check arguments */
	if (argc != 5) {
		usage(argv[0]);
		exit(1);
	}

	/* get device type */
	switch (argv[2][0]) {
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
			usage(argv[0]);
			exit(1);
	}

	/* get major/minor numbers */
	major = atoi(argv[3]);
	minor = atoi(argv[4]);

	/* create device */
	if (errno != ERANGE) {
		if (mknod(argv[1], mode, mkdev(major, minor)) < 0) {
			perror(argv[1]);
			exit(1);
		}
	}

	return 0;
}
