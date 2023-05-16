#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/*
 * Usage.
 */
static void usage(const char *name)
{
	fprintf(stderr, "Usage: %s seconds\n", name);
}

int main(int argc, char **argv)
{
	const char *name = argv[0];
	int seconds;

	/* check arguments */
	if (argc != 2) {
		usage(name);
		exit(1);
	}

	/* parse seconds */
	seconds = atoi(argv[1]);
	if (!seconds && strcmp(argv[1], "0") != 0) {
		usage(name);
		exit(1);
	}

	/* sleep */
	while ((seconds = sleep(seconds)) > 0);

	return 0;
}