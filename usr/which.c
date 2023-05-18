#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <getopt.h>
#include <sys/stat.h>

#include "libutils/utils.h"

/*
 * Look for a file in path.
 */
static int which(const char *name, const char *path)
{
	bool quit = false;
	char buf[BUFSIZ];
	char *p;

	/* copy path */
	strncpy(buf, path, BUFSIZ);
	path = buf;

	while (!quit) {
		/* find next path end */
		p = strchr(path, ':');
		if (!p)
			quit = true;
		else
			*p = 0;

		/* try concat(path, name) */
		sprintf(buf, "%s/%s", path, name);
		path = ++p;

		/* must be executable */
		if (access(buf, X_OK) == 0) {
			printf("%s\n", buf);
			break;
		}
	}


	return 0;
}

/*
 * Usage.
 */
static void usage(const char *name)
{
	fprintf(stderr, "Usage: %s [name ...]\n", name);
	fprintf(stderr, "\t  , --help\t\tprint help and exit\n");
}

/* options */
struct option long_opts[] = {
	{ "help",	no_argument,	0,	OPT_HELP	},
	{ 0,		0,		0,	0		},
};

int main(int argc, char **argv)
{
	const char *name = argv[0];
	int i, c, ret = 0;
	char *path;

	/* get options */
	while ((c = getopt_long(argc, argv, "", long_opts, NULL)) != -1) {
		switch (c) {
			case OPT_HELP:
				usage(name);
				exit(0);
				break;
			default:
				exit(1);
				break;
		}
	}

	/* skip options */
	argc -= optind;
	argv += optind;

	/* check arguments */
	if (argc < 1) {
		usage(name);
		exit(1);
	}

	/* get path */
	path = getenv("PATH");
	if (!path) {
		fprintf(stderr, "PATH is not set\n");
		exit(1);
	}

	/* compute all arguments */
	for (i = 0; i < argc; i++)
		ret |= which(argv[i], path);

	return ret;
}
