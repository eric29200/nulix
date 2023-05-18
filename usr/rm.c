#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <limits.h>
#include <errno.h>
#include <getopt.h>
#include <sys/stat.h>

#include "libutils/utils.h"

#define FLAG_FORCE		(1 << 0)
#define FLAG_INTERACT		(1 << 1)
#define FLAG_RECURSIVE		(1 << 2)

static int rm(const char *name, int flags, int level);

/*
 * Ask yes to user.
 */
static bool yes()
{
	int c, u;

	fflush(stdout);

	c = getchar();
	for (u = c; u != '\n' && u != EOF;)
		u = getchar();

	return c == 'y';
}

/*
 * "." or ".." ?
 */
static bool dotname(const char *name)
{
	return strcmp(name, ".") == 0 || strcmp(name, "..") == 0;
}

/*
 * Remove a directory.
 */
static int rm_dir(const char *name, int flags, int level)
{
	char subname[PATH_MAX];
	struct dirent *entry;
	int ret = 0;
	DIR *dirp;

	/* non recursive mode */
	if (!(flags & FLAG_RECURSIVE)) {
		fprintf(stderr, "rm: %s is a directory\n", name);
		return 1;
	}

	/* check write access */
	if (access(name, O_WRONLY) < 0) {
		if (!(flags & FLAG_FORCE))
			fprintf(stderr, "rm: %s not changed\n", name);

		return 1;
	}

	/* interactive rm */
	if ((flags & FLAG_INTERACT) && level != 0) {
		printf("rm: remove directory %s ? ", name);
		if (!yes())
			return 0;
	}

	/* open directory */
	dirp = opendir(name);
	if (!dirp) {
		fprintf(stderr, "rm: cannot open directory %s\n", name);
		return 0;
	}

	/* recursive rm */
	for (;;) {
		/* read next entry */
		entry = readdir(dirp);
		if (!entry)
			break;

		/* remove entry */
		if (entry->d_ino && !dotname(entry->d_name)) {
			snprintf(subname, PATH_MAX, "%s/%s", name, entry->d_name);
			ret += rm(subname, flags, level + 1);
		}
	}

	/* close directory */
	closedir(dirp);

	/* remove this directory */
	if (!dotname(name) && rmdir(name) < 0) {
		fprintf(stderr, "rm: %s", strerror(errno));
		return 1;
	}

	return !!ret;
}

/*
 * Remove a file.
 */
static int rm(const char *name, int flags, int level)
{
	struct stat statbuf;
	int ret;

	/* stat file */
	ret = lstat(name, &statbuf);
	if (ret < 0) {
		if (flags & FLAG_FORCE)
			return 0;
	
		fprintf(stderr, "rm: %s: no such file\n", name);
		return 1;
	}

	/* remove a directory */
	if (S_ISDIR(statbuf.st_mode))
		return rm_dir(name, flags, level);
	 
	/* interactive rm */
	if (flags & FLAG_INTERACT) {
		printf("rm: remove %s ? ", name);
		if (!yes())
			return 0;
	} else if (!S_ISLNK(statbuf.st_mode) && access(name, 02) < 0) {
		printf("rm: override protection for %s ? ", name);
		if (!yes())
			return 0;
	}

	/* remove */
	if (unlink(name) != 0 && (!(flags & FLAG_FORCE) || (flags & FLAG_INTERACT))) {
		fprintf(stderr, "rm: %s not removed\n", name);
		return 1;
	}

	return 0;
}
 
/*
 * Usage.
 */
static void usage(const char *name)
{
	fprintf(stderr, "Usage: %s [-rfi] file [...]\n", name);
	fprintf(stderr, "\t  , --help\t\tprint help and exit\n");
	fprintf(stderr, "\t-r, --recursive\t\tremove directories and their contents recursively\n");
	fprintf(stderr, "\t-f, --force\t\tignore nonexistent files and arguments, never prompt\n");
	fprintf(stderr, "\t-i, \t\t\tprompt before every removal\n");
}

/* options */
struct option long_opts[] = {
	{ "help",	no_argument,	0,	OPT_HELP	},
	{ "recursive",	no_argument,	0,	'r'		},
	{ "force",	no_argument,	0,	'f'		},
	{ 0,		no_argument,	0,	'i'		},
	{ 0,		0,		0,	0		},
};

int main(int argc, char **argv)
{
	int flags = 0, ret = 0, c, i;
	const char *name = argv[0];

	/* get options */
	while ((c = getopt_long(argc, argv, "fir", long_opts, NULL)) != -1) {
		switch (c) {
			case 'f':
				flags |= FLAG_FORCE;
				break;
			case 'i':
				flags |= FLAG_INTERACT;
				break;
			case 'r':
				flags |= FLAG_RECURSIVE;
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
	if (!argc) {
		usage(name);
		exit(1);
	}

	/* remove files */
	for (i = 0; i < argc; i++) {
		if (dotname(argv[i])) {
			fprintf(stderr, "%s : cannot remove '.' and '..'\n", name);
			continue;
		}

		ret += rm(argv[i], flags, 0);
	}

	return ret;
}
