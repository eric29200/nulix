#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <dirent.h>
#include <string.h>
#include <limits.h>
#include <sys/stat.h>

#include "libutils/build_path.h"

#define FLAG_HUMAN_READABLE	(1 << 0)

#define BLKSIZE			512
#define NR_BLKS(blocks)		((512 * (blocks) + (BLKSIZE) - 1) / (BLKSIZE))

static int du(const char *filename, int flags, int follow, int level, off_t *size);

/*
 * Print a size in most suitable format.
 */
static char *human_size(off_t size)
{
	static const char postfixes[] = "BKMGTPE";
	static char buf[32];
	size_t i;
	off_t n;

	for (n = size, i = 0; n >= 1024 && i < strlen(postfixes); i++)
		n /= 1024;

	if (!i)
		snprintf(buf, sizeof(buf), "%ju", (uintmax_t) size);
	else
		snprintf(buf, sizeof(buf), "%ju%c", n, postfixes[i]);

	return buf;
}

/*
 * Print a file size.
 */
static void print_size(const char *filename, off_t size, int flags)
{
	if (flags & FLAG_HUMAN_READABLE)
		printf("%s\t%s\n", human_size(size * BLKSIZE), filename);
	else
		printf("%jd\t%s\n", (intmax_t) size, filename);
}

/*
 * Compute size of a directory.
 */
static int du_dir(const char *dirname, int flags, int follow, int level, off_t *size)
{
	char file_path[PATH_MAX];
	struct dirent *entry;
	int ret = 0;
	DIR *dirp;
	off_t s;
	
	/* open directory */
	dirp = opendir(dirname);
	if (!dirp) {
		perror(dirname);
		return 1;
	}

	/* read directory */
	while ((entry = readdir(dirp))) {
		/* skip "." and ".." */
		if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
			continue;

		/* build file path */
		if (build_path((char *) dirname, entry->d_name, file_path, PATH_MAX)) {
			ret = 1;
			continue;
		}

		/* recursive du */
		ret |= du(file_path, flags, follow, level + 1, &s);

		/* update size */
		*size += s;
	}

	/* close directory */
	closedir(dirp);

	return ret;
}

/*
 * Compute size of a file/directory.
 */
static int du(const char *filename, int flags, int follow, int level, off_t *size)
{
	int stat_flags = 0, ret = 0;
	struct stat st;

	/* reset size */
	*size = 0;
	
	/* follow links ? */
	if (follow == 'P' || (follow == 'H' && level))
		stat_flags |= AT_SYMLINK_NOFOLLOW;

	/* stat file */
	if (fstatat(AT_FDCWD, filename, &st, stat_flags) < 0) {
		perror(filename);
		return 1;
	}

	/* get size of this file */ 
	*size = NR_BLKS(st.st_blocks);

	/* compute directory recursively */
	if (S_ISDIR(st.st_mode) && du_dir(filename, flags, follow, level, size)) {
		fprintf(stderr, "%s: can't get size\n", filename);
		return 1;
	}

	/* print size */
	if (!level || S_ISDIR(st.st_mode))
		print_size(filename, *size, flags);

	return ret;
}

/*
 * Usage.
 */
static void usage(const char *name)
{
	fprintf(stderr, "Usage: %s [-ahHLP] [file ...]\n", name);
}

int main(int argc, char **argv)
{
	int flags = 0, follow = 'P', ret = 0;
	const char *name = argv[0];
	off_t size;
	char *p;

	/* skip program name */
	argc--;
	argv++;

	/* parse arguments */
	while (*argv && argv[0][0] == '-') {
		for (p = &argv[0][1]; *p; p++) {
			switch (*p) {
				case 'h':
					flags |= FLAG_HUMAN_READABLE;
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

	/* compute current directory */
	if (!argc)
		return du(".", flags, follow, 0, &size);

	/* compute arguments */
	while (argc--)
		ret |= du(*argv++, flags, follow, 0, &size);

	return ret;
}