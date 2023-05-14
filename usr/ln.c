#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/stat.h>
 
static char buf[PATH_MAX];

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
 * Build link name.
 */
static char *build_link_name(const char *name, char *target)
{
	char *s;

	if (!name || *name == 0)
		return target;

	s = strrchr(target, '/');
	if (s)
		target = s + 1;

	strcpy(buf, name);
	strcat(buf, "/");
	strcat(buf, target);

	return buf;
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
	char *lastarg, *target, *name;
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
		target = argv[i];
		if (access(target, 0) < 0) {
			perror(target);
			continue;
		}

		/* build link name */
		name = lastarg;
		if (dirflag)
			name = build_link_name(name, target);

		/* create link */
		if (link(target, name) < 0) {
			perror(name);
			continue;
		}
	}

	return 0;
}
