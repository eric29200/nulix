#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/stat.h>
#include <getopt.h>

#include "libutils/opt.h"
#include "libutils/build_path.h"
 
/*
 * Check if name is a directory.
 */
static bool isdir(const char *name)
{
	struct stat statbuf;

	if (stat(name, &statbuf) < 0)
		return false;

	return S_ISDIR(statbuf.st_mode);
}

/*
 * Usage.
 */
static void usage(const char *name)
{
	fprintf(stderr, "Usage: %s [-s] link_target link_name\n", name);
	fprintf(stderr, "\t-s, --symbolic\t\tcreate symbolic links\n");
	fprintf(stderr, "\t  , --help\t\tprint help and exit\n");
}

/* options */
struct option long_opts[] = {
	{ "help",	no_argument,	0,	OPT_HELP	},
	{ "symbolic",	no_argument,	0,	's'		},
	{ 0,		0,		0,	0		},
};

int main(int argc, char **argv)
{
	char *lastarg, *srcname, dstname[PATH_MAX];
	const char *name = argv[0];
	int i, c, symbolic = 0;
	bool dirflag;

	/* get options */
	while ((c = getopt_long(argc, argv, "s", long_opts, NULL)) != -1) {
		switch (c) {
			case 's':
				symbolic = 1;
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

	/* check arguments */
	if (argc < 2) {
		usage(name);
		exit(1);
	}

	/* create symbolic link */
	if (symbolic) {
		if (argc != 2) {
			usage(name);
			exit(1);
		}

		/* create symbolic link */
		if (symlink(argv[0], argv[1]) < 0) {
			perror(argv[0]);
			exit(1);
		}

		return 0;
	}

	/* mutiple arguments : last argument must be a directory */
	lastarg = argv[argc - 1];
	dirflag = isdir(lastarg);
	if (argc > 2 && !dirflag) {
		fprintf(stderr, "%s: not a directory\n", lastarg);
		exit(1);
	}

	for (i = 0; i < argc - 1; i++) {
		/* check target link */
		srcname = argv[i];
		if (access(srcname, 0) < 0) {
			perror(srcname);
			continue;
		}

		/* build link name */
		if (dirflag)
			build_path(lastarg, srcname, dstname, PATH_MAX);
		else
			strncpy(dstname, lastarg, PATH_MAX);

		printf("%s -> %s\n", srcname, dstname);

		/* create link */
		if (link(srcname, dstname) < 0) {
			perror(dstname);
			continue;
		}
	}

	return 0;
}
