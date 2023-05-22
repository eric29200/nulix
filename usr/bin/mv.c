#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <getopt.h>

#include "../libutils/libutils.h"

#define FLAG_FORCE	(1 << 0)
#define FLAG_VERBOSE	(1 << 1)

/*
 * Move a file.
 */
static int mv(const char *src, const char *dst, int flags)
{
	int ret;

	/* print move */
	if (flags & FLAG_VERBOSE)
		printf("'%s' -> '%s'\n", src, dst);

	/* try to rename */
	ret = rename(src, dst);
	if (!ret)
		return 0;

	return 1;
}

/*
 * Usage.
 */
static void usage(const char *name)
{
	fprintf(stderr, "Usage: %s [-fv] source [...] target\n", name);
	fprintf(stderr, "\t  , --help\t\tprint help and exit\n");
	fprintf(stderr, "\t-f, --force\t\tdo not ask confirmation\n");
	fprintf(stderr, "\t-v, --verbose\t\texplain what is being done\n");
}

/* options */
struct option long_opts[] = {
	{ "help",	no_argument,	0,	OPT_HELP	},
	{ "force",	no_argument,	0,	'f'		},
	{ "verbose",	no_argument,	0,	'v'		},
	{ 0,		0,		0,	0		},
};

int main(int argc, char **argv)
{
	const char *name = argv[0];
	int flags = 0, ret = 0, c;
	char buf[BUFSIZ];
	char *src, *dst;
	bool dst_is_dir;
	struct stat st;

	/* get options */
	while ((c = getopt_long(argc, argv, "", long_opts, NULL)) != -1) {
		switch (c) {
			case 'f':
				flags |= FLAG_FORCE;
				break;
			case 'v':
				flags |= FLAG_VERBOSE;
				break;
			case  OPT_HELP:
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

	/* check if dst is a directory */
	dst = argv[argc - 1];
	dst_is_dir = stat(dst, &st) == 0 && S_ISDIR(st.st_mode);

	/* simple move */
	if (argc == 2 && !dst_is_dir)
		return mv(argv[0], dst, flags);

	/* multiple move : dst must be a directory */
	if (argc > 2 && !dst_is_dir) {
		fprintf(stderr, "%s: not a directory\n", dst);
		exit(1);
	}

	/* move all files */
	while (argc-- > 1) {
		src = *argv++;

		/* build destination path */
		if (build_path(dst, src, buf, BUFSIZ)) {
			ret = 1;
			continue;
		}

		/* move file */
		ret |= mv(src, buf, flags);
	}

	return ret;
}
