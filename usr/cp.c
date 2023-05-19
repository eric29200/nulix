#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>
#include <fcntl.h>
#include <dirent.h>
#include <string.h>
#include <utime.h>
#include <libgen.h>
#include <stdbool.h>
#include <getopt.h>
#include <sys/stat.h>

#include "libutils/utils.h"

/*
 * Usage.
 */
static void usage(const char *name)
{
	fprintf(stderr, "Usage: %s [-RvfapHLP] source [...] target\n", name);
	fprintf(stderr, "\t  , --help\t\t\tprint help and exit\n");
	fprintf(stderr, "\t-R, --recursive\t\t\tcopy directories recursiverly\n");
	fprintf(stderr, "\t-v, --verbose\t\t\texplain what is being done\n");
	fprintf(stderr, "\t-f, --force\t\t\tif any existing file cannot be opened, remove it and try again\n");
	fprintf(stderr, "\t-a, --archive\t\t\tpreserve all attributes\n");
	fprintf(stderr, "\t-p,\t\t\t\tpreserve mode, ownership and timestamps\n");
	fprintf(stderr, "\t-H, --dereference-args\t\tdereference only symlinks from command line\n");
	fprintf(stderr, "\t-L, --dereference\t\tderference all symbolic links\n");
	fprintf(stderr, "\t-P, --no-dereference\t\tdon't follow any symbolic links (default)\n");
}

/* options */
struct option long_opts[] = {
	{ "help",		no_argument,	0,	OPT_HELP	},
	{ "recursive",		no_argument,	0,	'R'		},
	{ "verbose",		no_argument,	0,	'v'		},
	{ "force",		no_argument,	0,	'f'		},
	{ "archive",		no_argument,	0,	'a'		},
	{ 0,			no_argument,	0,	'p'		},
	{ "dereference-args",	no_argument,	0,	'H'		},
	{ "dereference",	no_argument,	0,	'L'		},
	{ "dereference",	no_argument,	0,	'P'		},
	{ 0,			0,		0,	0		},
};

int main(int argc, char **argv)
{
	int flags = 0, ret = 0, follow = 0, c;
	const char *name = argv[0];
	char buf[BUFSIZ];
	char *src, *dst;
	bool dst_is_dir;
	struct stat st;

	/* get options */
	while ((c = getopt_long(argc, argv, "RvfapHLP", long_opts, NULL)) != -1) {
		switch (c) {
			case 'f':
				flags |= FLAG_CP_FORCE;
				break;
			case 'R':
			case 'r':
				flags |= FLAG_CP_RECURSIVE;
				break;
			case 'v':
				flags |= FLAG_CP_VERBOSE;
				break;
			case 'a':
				flags |= FLAG_CP_RECURSIVE;
				flags |= FLAG_CP_PRESERVE_ATTR;
				flags |= FLAG_CP_PRESERVE_MODE;
				break;
			case 'p':
				flags |= FLAG_CP_PRESERVE_MODE;
				break;
			case 'H':
			case 'L':
			case 'P':
				follow = c;
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

	/* default follow */
	if (!follow)
		follow = flags & FLAG_CP_RECURSIVE ? 'P' : 'L';

	/* check if dst is a directory */
	dst = argv[argc - 1];
	dst_is_dir = stat(dst, &st) == 0 && S_ISDIR(st.st_mode);

	/* simple copy */
	if (argc == 2 && !dst_is_dir)
		return __copy(argv[0], dst, flags, follow, 0);

	/* multiple copy : dst must be a directory */
	if (argc > 2 && !dst_is_dir) {
		fprintf(stderr, "%s: not a directory\n", dst);
		exit(1);
	}

	/* copy all files */
	while (argc-- > 1) {
		src = *argv++;

		/* build destination path */
		if (__build_path(dst, src, buf, BUFSIZ)) {
			ret = 1;
			continue;
		}

		/* copy file */
		ret |= __copy(src, buf, flags, follow, 0);
	}

	return ret;
}
