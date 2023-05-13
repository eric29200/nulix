#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

int main(int argc, char **argv)
{
	mode_t mode = 0;
	char *mode_str;
	int i;

	/* check arguments */
	if (argc < 3) {
		fprintf(stderr, "Usage: %s mode file [...]\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	/* get mode */
	for (mode_str = argv[1]; *mode_str >= '0' && *mode_str < '8'; mode_str++)
		mode = mode * 8 + (*mode_str - '0');

	/* check mode */
	if (*mode_str) {
		fprintf(stderr, "Mode must be specified as an octal number\n");
		exit(EXIT_FAILURE);
	}

	/* chmod all files */
	for (i = 2; i < argc; i++)
		if (chmod(argv[i], mode) < 0)
			perror(argv[i]);

	return 0;
}