#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/stat.h>

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
}

int main(int argc, char **argv)
{
	char *lastarg, *srcname, dstname[PATH_MAX];
	bool dirflag;
	int i;

	/* check arguments */
	if (argc < 3) {
		usage(argv[0]);
		exit(1);
	}

	/* symbolic link */
	if (argv[1][0] == '-') {
		if (strcmp(argv[1], "-s") != 0 || argc != 4) {
			usage(argv[0]);
			exit(1);
		}

		/* create symbolic link */
		if (symlink(argv[2], argv[3]) < 0) {
			perror(argv[3]);
			exit(1);
		}

		return 0;
	}

	/* mutiple arguments : last argument must be a directory */
	lastarg = argv[argc - 1];
	dirflag = isdir(lastarg);
	if (argc > 3 && !dirflag) {
		fprintf(stderr, "%s: not a directory\n", lastarg);
		exit(1);
	}

	for (i = 1; i < argc - 1; i++) {
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
