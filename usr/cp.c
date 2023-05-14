#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <utime.h>
#include <dirent.h>
#include <sys/stat.h>

#define FLAG_RECURSIVE		(1 << 0)
#define FLAG_FORCE		(1 << 1)
#define FLAG_VERBOSE		(1 << 2)

static int copy_item(const char *src, const char *dst, int flags);

static char buf[BUFSIZ];

/*
 * "." or ".." ?
 */
static bool dotname(const char *name)
{
	return strcmp(name, ".") == 0 || strcmp(name, "..") == 0;
}

/*
 * Build a file name.
 */
static char *build_name(const char *dirname, const char *filename)
{
	size_t filename_len = strlen(filename);
	size_t dirname_len = strlen(dirname);
	char *s;

	/* allocate filename */
	s = (char *) malloc(dirname_len + filename_len + 2);
	if (!s)
		return NULL;

	/* concat dirname and filename */
	memcpy(s, dirname, dirname_len);
	s[dirname_len] = '/';
	memcpy(s + dirname_len + 1, filename, filename_len);
	s[dirname_len + 1 + filename_len] = 0;

	return s;
}

/*
 * Copy a directory.
 */
static int copy_dir(const char *src, const char *dst, struct stat *src_statbuf, int flags)
{
	char *src_file, *dst_file;
	struct dirent *entry;
	int ret = 0;
	DIR *dirp;

	/* create directory */
	mkdir(dst, src_statbuf->st_mode & 0777);

	/* open src directory */
	dirp = opendir(src);

	/* copy all entries */
	while ((entry = readdir(dirp)) != NULL) {
		/* skip "." and ".." */
		if (dotname(entry->d_name))
			continue;

		/* build src path */
		src_file = build_name(src, entry->d_name);
		if (!src_file) {
			ret = 1;
			continue;
		}

		/* build dst path */
		dst_file = build_name(dst, entry->d_name);
		if (!dst_file) {
			free(src_file);
			ret = 1;
			continue;
		}

		/* copy item */
		ret |= copy_item(src_file, dst_file, flags);

		/* free paths */
		free(src_file);
		free(dst_file);
	}

	/* close src directory */
	closedir(dirp);

	return ret;
}

/*
 * Copy a file.
 */
static int copy_file(const char *src, const char *dst, struct stat *src_statbuf, int flags)
{
	struct stat dst_statbuf;
	int rfd, wfd, rcc, wcc;
	struct utimbuf times;
	char *bp;

	/* print copy */
	if (flags & FLAG_VERBOSE)
		printf("'%s' -> '%s'\n", src, dst);

	/* stat dst file */
	if (stat(dst, &dst_statbuf) < 0) {
		dst_statbuf.st_ino = -1;
		dst_statbuf.st_dev = -1;
	}

	/* same file : error */
	if (src_statbuf->st_ino == dst_statbuf.st_ino && src_statbuf->st_dev == dst_statbuf.st_dev) {
		fprintf(stderr, "cp: '%s' and '%s' are the same file\n", src, dst);
		return 1;
	}

	/* open src file */
	rfd = open(src, O_RDONLY);
	if (rfd < 0) {
		perror(src);
		return 1;
	}

	/* open dst file */
	wfd = open(dst, O_WRONLY | O_CREAT);
	if (wfd < 0) {
		perror(dst);
		close(rfd);
		return 1;
	}

	/* copy content */
	while ((rcc = read(rfd, buf, BUFSIZ)) > 0) {
		bp = buf;

		while (rcc > 0) {
			wcc = write(wfd, bp, rcc);
			if (wcc < 0) {
				perror(dst);
				goto err;
			}

			bp += wcc;
			rcc -= wcc;
		}
	}

	/* read error */
	if (rcc < 0) {
		perror(src);
		goto err;
	}

	/* close files */
	close(rfd);
	close(wfd);

	/* set mode, uid, gid */
	chmod(dst, src_statbuf->st_mode);
	chown(dst, src_statbuf->st_uid, src_statbuf->st_gid);

	/* set times */
	times.actime = src_statbuf->st_atime;
	times.modtime = src_statbuf->st_mtime;
	utime(dst, &times);

	return 0;
err:
	close(rfd);
	close(wfd);
	return 1;
}

/*
 * Copy an item.
 */
static int copy_item(const char *src, const char *dst, int flags)
{
	struct stat src_statbuf;

	/* stat src file */
	if (stat(src, &src_statbuf) < 0) {
		perror(src);
		return 1;
	}

	/* copy directory */
	if (S_ISDIR(src_statbuf.st_mode)) {
		if (flags & FLAG_RECURSIVE)
			return copy_dir(src, dst, &src_statbuf, flags);

		fprintf(stderr, "cp: -r not specified: omitting directory '%s'\n", src);
		return 0;
	}
	
	return copy_file(src, dst, &src_statbuf, flags);
}

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
	fprintf(stderr, "Usage: %s [-Rfv] source [...] target\n", name);
}

int main(int argc, char **argv)
{
	char *lastarg, *p, *src, *dst;
	const char *name = argv[0];
	int flags = 0, ret = 0;
	bool dirflag;

	/* parse options */
	while (argv[1] && argv[1][0] == '-') {
		for (p = &argv[1][1]; *p; p++) {
			switch (*p) {
				case 'r':
					flags |= FLAG_RECURSIVE;
					break;
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

	/* check arguments */
	argc--;
	argv++;
	if (argc < 2) {
		usage(name);
		exit(1);
	}

	/* mutiple arguments : last argument must be a directory */
	lastarg = argv[argc - 1];
	dirflag = isdir(lastarg);
	if (argc > 2 && !dirflag) {
		fprintf(stderr, "%s: not a directory\n", lastarg);
		exit(1);
	}

	/* copy files */
	while (argc-- > 1) {
		src = *argv++;
		dst = lastarg;

		/* copy to directory */
		if (dirflag)
			dst = build_name(dst, src);

		/* copy item */
		if (copy_item(src, dst, flags)) {
			fprintf(stderr, "Failed to copy %s -> %s\n", src, dst);
			ret = 1;
		}

		/* free path */
		if (dirflag)
			free(dst);
	}

	return ret;
}
