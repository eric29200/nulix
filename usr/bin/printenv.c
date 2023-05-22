#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>

#include "../libutils/libutils.h"

/*
 * Usage.
 */
static void usage(const char *name)
{
	fprintf(stderr, "Usage: %s [variable]\n", name);
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
	char **env = environ;
	char *eptr;
	size_t len;
	int c;

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

	/* print all */
	if (!argc) {
		while (*env) {
			eptr = *env++;
			printf("%s\n", eptr);
		}

		return 0;
	}

	/* find variable */
	len = strlen(argv[0]);
	while (*env) {
		if (strlen(*env) > len && env[0][len] == '=' && memcmp(argv[0], *env, len) == 0) {
			eptr = &env[0][len + 1];
			printf("%s\n", eptr);
			return 0;
		}

		env++;
	}

	return 1;
}
