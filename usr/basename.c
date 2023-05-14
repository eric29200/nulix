#include <stdio.h>
#include <libgen.h>
#include <stdlib.h>
#include <string.h>

/*
 * Remove suffix.
 */
static void remove_suffix(char *name, char *suffix)
{
	char *np, *sp;

	/* go to end */
	np = name + strlen(name);
	sp = suffix + strlen(suffix);

	while (np > name && sp > suffix)
		if (*--np != *--sp)
			return;

	if (np > name)
		*np = 0;
}

/*
 * Usage.
 */
static void usage(const char *name)
{
	fprintf(stderr, "Usage: %s path [suffix]\n", name);
}

int main(int argc, char **argv)
{
	char *b;

	/* check arguments */
	if (argc != 2 && argc != 3) {
		usage(argv[0]);
		exit(1);
	}

	/* get basename */
	b = basename(argv[1]);

	/* remove suffix */
	if (argc == 3)
		remove_suffix(b, argv[2]);

	/* print result */
	printf("%s\n", b);

	return 0;
}
