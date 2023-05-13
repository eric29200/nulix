#include <stdio.h>
#include <libgen.h>
#include <stdlib.h>

/*
 * Usage.
 */
static void usage(const char *name)
{
	fprintf(stderr, "Usage: %s path\n", name);
}

int main(int argc, char **argv)
{
	char *d;

	/* check arguments */
	if (argc != 2) {
		usage(argv[0]);
		exit(EXIT_FAILURE);
	}

	/* get dirname */
	d = dirname(argv[1]);
	printf("%s\n", d);

	return 0;
}
