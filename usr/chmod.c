#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

/*
 * Usage.
 */
static void usage(const char *name)
{
	fprintf(stderr, "Usage: %s mode file [...]\n", name);
}

int main(int argc, char **argv)
{
	mode_t mode = 0;
	char *mode_str;
	int i;

	/* check arguments */
	if (argc < 3) {
		usage(argv[0]);
		exit(1);
	}

	/* get mode */
	for (mode_str = argv[1]; *mode_str >= '0' && *mode_str < '8'; mode_str++)
		mode = mode * 8 + (*mode_str - '0');

	/* check mode */
	if (*mode_str) {
		fprintf(stderr, "Mode must be specified as an octal number\n");
		exit(1);
	}

	/* chmod all files */
	for (i = 2; i < argc; i++)
		if (chmod(argv[i], mode) < 0)
			perror(argv[i]);

	return 0;
}
