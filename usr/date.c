#include <stdio.h>
#include <stdlib.h>
#include <time.h>

/*
 * Usage.
 */
static void usage(const char *name)
{
	fprintf(stderr, "Usage: %s\n", name);
}

int main(int argc, char **argv)
{
	char *format = "%a %b %e %H:%M:%S %Z %Y";
	const char *name = argv[0];
	char buf[BUFSIZ];
	struct tm *tm;
	time_t t;

	/* skip program name */
	argc--;
	argv++;

	/* check arguments */
	if (argc) {
		usage(name);
		exit(1);
	}

	/* get current time */
	t = time(NULL);
	if (t == 1) {
		perror(name);
		exit(1);
	}

	/* get localtime */
	tm = localtime(&t);
	if (!tm) {
		perror(name);
		exit(1);
	}

	/* print format */
	strftime(buf, BUFSIZ, format, tm);
	printf("%s\n", buf);

	return 0;
}