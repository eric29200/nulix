#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <limits.h>
#include <errno.h>
#include <sys/stat.h>

#define FLAG_FORCE		(1 << 0)
#define FLAG_INTERACT		(1 << 1)
#define FLAG_RECURSIVE		(1 << 2)

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
 * Remove a file.
 */
static int rm(const char *name, int flags, int level)
{
	char subname[PATH_MAX];
	struct dirent *entry;
	struct stat statbuf;
	DIR *dirp;
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
	if (S_ISDIR(statbuf.st_mode)) {
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
	if (unlink(name) && (!(flags & FLAG_FORCE) || (flags & FLAG_INTERACT))) {
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
}

int main(int argc, char **argv)
{
	int i, j, flags = 0, ret = 0;

	/* check arguments */
	if (argc < 2) {
		usage(argv[0]);
		exit(EXIT_FAILURE);
	}

	/* parse arguments */
	for (i = 1; i < argc && argv[i][0] == '-'; i++) {
		for (j = 1; argv[i][j]; j++) {
			switch (argv[i][j]) {
				case 0:
					break;
				case 'f':
					flags |= FLAG_FORCE;
					break;
				case 'i':
					flags |= FLAG_INTERACT;
					break;
				case 'r':
					flags |= FLAG_RECURSIVE;
					break;
				default:
					usage(argv[0]);
					exit(EXIT_FAILURE);
			}
		}
	}

	/* remove files */
	for (; i < argc; i++) {
		if (dotname(argv[i])) {
			fprintf(stderr, "%s : cannot remove '.' and '..'\n", argv[0]);
			continue;
		}

		ret += rm(argv[i], flags, 0);
	}

	return ret;
}