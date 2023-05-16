#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/*
 * Usage.
 */
static void usage(const char *name)
{
	fprintf(stderr, "Usage: %s file\n", name);
}

int main(int argc, char **argv)
{
	const char *name = argv[0];

	if (argc != 2) {
		usage(name);
		exit(1);
	}

	if (unlink(argv[1]) < 0) {
		perror("link");
		return 1;
	}

	return 0;
}