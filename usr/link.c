#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/*
 * Usage.
 */
static void usage(const char *name)
{
	fprintf(stderr, "Usage: %s target name\n", name);
}

int main(int argc, char **argv)
{
	const char *name = argv[0];

	if (argc != 3) {
		usage(name);
		exit(1);
	}

	if (link(argv[1], argv[2]) < 0) {
		perror("link");
		return 1;
	}

	return 0;
}