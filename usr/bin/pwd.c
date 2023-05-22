#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <getopt.h>
#include <sys/stat.h>

#include "../libutils/libutils.h"

/*
 * Usage.
 */
static void usage(const char *name)
{
	fprintf(stderr, "Usage: %s [-LP]\n", name);
	fprintf(stderr, "\t  , --help\t\tprint help and exit\n");
	fprintf(stderr, "\t-L, --logical\t\tuse PWD from environment\n");
	fprintf(stderr, "\t-P, --physical\t\tavoid symbolic links\n");
}

/* options */
struct option long_opts[] = {
	{ "help",	no_argument,	0,	OPT_HELP	},
	{ "logical",	no_argument,	0,	'L'		},
	{ "physical",	no_argument,	0,	'P'		},
	{ 0,		0,		0,	0		},
};

int main(int argc, char **argv)
{
	const char *name = argv[0];
	char buf[PATH_MAX];
	int mode = 'L', c;
	const char *pwd;
	struct stat st;

	/* get options */
	while ((c = getopt_long(argc, argv, "LP", long_opts, NULL)) != -1) {
		switch (c) {
			case 'L':
			case 'P':
				mode = c;
				break;
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

	/* get current directory */
	if (!getcwd(buf, PATH_MAX))
		perror(name);

	/* use pwd from env */
	if (mode == 'L' && (pwd = getenv("PWD")) && pwd[0] == '/' && stat(pwd, &st) == 0)
		strncpy(buf, pwd, PATH_MAX);

	/* print */
	printf("%s\n", buf);

	return 0;
}
