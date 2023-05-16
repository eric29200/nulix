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
#include <sys/stat.h>

#include "libutils/build_path.h"
#include "libutils/copy.h"

/*
 * Usage.
 */
static void usage(const char *name)
{
	fprintf(stderr, "Usage: %s [-RvfapHLP] source [...] target\n", name);
}

int main(int argc, char **argv)
{
	int flags = 0, ret = 0, follow = 0;
	const char *name = argv[0];
	char *p, *src, *dst;
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
				 	follow = *p;
					break;
				default:
					usage(name);
					exit(1);
			}
		}

		argc--;
		argv++;
	}

	/* default follow */
	if (!follow)
		follow = flags & FLAG_CP_RECURSIVE ? 'P' : 'L';

	/* update arguments position */
	argv++;
	if (--argc < 2) {
		usage(name);
		exit(1);
	}

	/* check if dst is a directory */
	dst = argv[argc - 1];
	dst_is_dir = stat(dst, &st) == 0 && S_ISDIR(st.st_mode);

	/* simple copy */
	if (argc == 2 && !dst_is_dir)
		return copy(argv[0], dst, flags, follow, 0);

	/* multiple copy : dst must be a directory */
	if (argc > 2 && !dst_is_dir) {
		fprintf(stderr, "%s: not a directory\n", dst);
		exit(EXIT_FAILURE);
	}

	/* copy all files */
	while (argc-- > 1) {
		src = *argv++;
		
		/* build destination path */
		if (build_path(dst, src, buf, BUFSIZ)) {
			ret = 1;
			continue;
		}

		/* copy file */
		ret |= copy(src, buf, flags, follow, 0);
	}

	return ret;
}