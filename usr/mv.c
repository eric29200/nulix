#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/stat.h>

#include "libutils/build_path.h"

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
}

int main(int argc, char **argv)
{
	const char *name = argv[0];
	int flags = 0, ret = 0;
	char *src, *dst, *p;
	char buf[BUFSIZ];
	bool dst_is_dir;
	struct stat st;

	/* check arguments */
	if (argc < 3) {
		usage(argv[0]);
		exit(1);
	}

	/* parse options */
	while (argv[1] && argv[1][0] == '-') {
		for (p = &argv[1][1]; *p; p++) {
			switch (*p) {
				case 'f':
					flags |= FLAG_FORCE;
					break;
				case 'v':
					flags |= FLAG_VERBOSE;
					break;
				default:
					usage(name);
					exit(1);
			}
		}

		argc--;
		argv++;
	}

	/* update arguments position */
	argv++;
	if (--argc < 2) {
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