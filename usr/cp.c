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

#define FLAG_FORCE		(1 << 0)
#define FLAG_VERBOSE		(1 << 1)
#define FLAG_RECURSIVE		(1 << 2)
#define FLAG_PRESERVE_ATTR	(1 << 3)
#define FLAG_PRESERVE_MODE	(1 << 4)

static int follow = 0;

static int cp(const char *src, const char *dst, int flags, int level);

/*
 * Build a path.
 */
static int build_path(char *dirname, char *filename, char *buf, int bufsiz)
{
	size_t dirname_len = strlen(dirname);
	int len, i;

	/* remove end slashes in dirname */
	for (i = dirname_len - 1; i && dirname[i] == '/'; i--)
		dirname[i] = 0;

	/* concat dirname and filename */
	len = snprintf(buf, bufsiz, "%s/%s", dirname, basename(filename));

	/* filename too long */
	if (len < 0 || len >= bufsiz) {
		fprintf(stderr, "'%s%s': filename too long\n", dirname, basename(filename));
		return 1;
	}

	return 0;
}


/*
 * Copy a link.
 */
static int cp_link(const char *src, const char *dst, int flags)
{
	char target[PATH_MAX];
	int n;

	/* read target link */
	n = readlink(src, target, PATH_MAX - 1);
	if (n < 0)
		return 1;
	target[n] = 0;

	/* unlink destination */
	if ((flags & FLAG_FORCE) && unlink(dst) < 0 && errno != ENOENT) {
		perror(dst);
		return 1;
	}
		
	/* create new link */
	if (symlink(target, dst) < 0) {
		perror(dst);
		return 1;
	}

	return 0;
}

/*
 * Copy a node.
 */
static int cp_nod(const struct stat *src_statbuf, const char *dst, int flags)
{
	/* unlink destination */
	if ((flags & FLAG_FORCE) && unlink(dst) < 0 && errno != ENOENT) {
		perror(dst);
		return 1;
	}
		
	/* create new link */
	if (mknod(dst, src_statbuf->st_mode, src_statbuf->st_rdev) < 0) {
		perror(dst);
		return 1;
	}

	return 0;
}

/*
 * Copy a regular file.
 */
static int cp_file(const char *src, const char *dst, int flags, const struct stat *src_statbuf)
{
	int sfd, dfd, sn, dn, ret = 0;
	char buf[BUFSIZ], *bp;

	/* open src file */
	sfd = open(src, O_RDONLY);
	if (sfd < 0) {
		perror(src);
		return 1;
	}

	/* create dst file */
	dfd = creat(dst, src_statbuf->st_mode);

	/* create failed : remove dst file and retry */
	if (dfd < 0 && (flags & FLAG_FORCE)) {
		if (unlink(dst) < 0 && errno != ENOENT) {
			perror(dst);
			close(sfd);
			return 1;
		}

		/* retry to create */
		dfd = creat(dst, src_statbuf->st_mode);
	}

	/* no way to create dst file */
	if (dfd < 0) {
		fprintf(stderr, "Can't create '%s'\n", dst);
		close(sfd);
		return 1;
	}

	/* copy content */
	while ((sn = read(sfd, buf, BUFSIZ)) > 0) {
		bp = buf;

		while (sn > 0) {
			dn = write(dfd, buf, sn);
			if (dn < 0) {
				perror(dst);
				ret = 1;
				goto out;
			}

			bp += dn;
			sn -= dn;
		}
	}

	/* handle read error */
	if (sn < 0) {
		perror(src);
		ret = 1;
		goto out;
	}

 out:
	close(sfd);
	close(dfd);
	return ret;
}

/*
 * Copy a directory.
 */
static int cp_dir(const char *src, const char *dst, int flags, int level, const struct stat *src_statbuf)
{
	char src_path[PATH_MAX], dst_path[PATH_MAX];
	struct dirent *entry;
	int ret = 0;
	DIR *dirp;

	/* check recursive flag */
	if (!(flags & FLAG_RECURSIVE)) {
		fprintf(stderr, "-r not specified ; omitting directory '%s'\n", src);
		return 1;
	}

	/* open directory */
	dirp = opendir(src);
	if (!dirp) {
		perror(src);
		return 1;
	}

	/* create directory if needed */
	if (mkdir(dst, src_statbuf->st_mode) < 0 && errno != EEXIST) {
		perror(dst);
		closedir(dirp);
		return 1;
	}

	/* copy all entries */
	while ((entry = readdir(dirp)) != NULL) {
		/* skip '.' and '..' */
		if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
			continue;

		/* build src path */
		if (build_path((char *) src, entry->d_name, src_path, PATH_MAX)) {
			ret = 1;
			continue;
		}

		/* build dst path */
		if (build_path((char *) dst, entry->d_name, dst_path, PATH_MAX)) {
			ret = 1;
			continue;
		}

		/* recusrive copy */
		ret |= cp(src_path, dst_path, flags, level + 1);
	}

	/* close directory */
	closedir(dirp);

	return ret;
}

/*
 * Copy.
 */
static int cp(const char *src, const char *dst, int flags, int level)
{
	struct stat st, st_src, st_dst;
	int stat_flags = 0, ret;
	struct utimbuf times;

	/* same file */
	if (stat(src, &st_src) == 0
	    && stat(dst, &st_dst) == 0
	    && st_src.st_dev == st_dst.st_dev
	    && st_src.st_ino == st_dst.st_ino) {
		fprintf(stderr, "'%s' and '%s' are the same file\n", src, dst);
		return 1;
	}

	/* don't follow links ? */
	if (follow == 'P' || (follow == 'H' && level))
		stat_flags |= AT_SYMLINK_NOFOLLOW;
	
	/* stat src file */
	if (fstatat(AT_FDCWD, src, &st, stat_flags) < 0) {
		perror(src);
		return 1;
	}
	
	/* print copy */
	if (flags & FLAG_VERBOSE)
		printf("'%s' -> '%s'\n", src, dst);

	/* copy */
	if (S_ISLNK(st.st_mode))
		ret = cp_link(src, dst, flags);
	else if (S_ISDIR(st.st_mode))
		ret = cp_dir(src, dst, flags, level, &st);
	else if ((flags & FLAG_PRESERVE_ATTR) && (S_ISBLK(st.st_mode) || S_ISCHR(st.st_mode) || S_ISSOCK(st.st_mode) || S_ISFIFO(st.st_mode)))
		ret = cp_nod(&st, dst, flags);
	else
		ret = cp_file(src, dst, flags, &st);

	/* on error return */
	if (ret)
		return ret;

	/* preserve attributes */
	if ((flags & FLAG_PRESERVE_ATTR) || (flags & FLAG_PRESERVE_MODE)) {
		/* preserve time */
		times.actime = st.st_atime;
		times.modtime = st.st_mtime;
		if (utime(dst, &times) < 0) {
			perror(dst);
			ret = 1;
		}

		/* preserve owner/mode */
		if (!S_ISLNK(st.st_mode)) {
			if (chown(dst, st.st_uid, st.st_gid) < 0) {
				perror(dst);
				ret = 1;
			}

			if (chmod(dst, st.st_mode) < 0) {
				perror(dst);
				ret = 1;
			}
		} else if (lchown(dst, st.st_uid, st.st_gid) < 0) {
			perror(dst);
			ret = 1;
		}
	}

	return ret;
}

/*
 * Usage.
 */
static void usage(const char *name)
{
	fprintf(stderr, "Usage: %s [-RvfapHLP] source [...] target\n", name);
}

int main(int argc, char **argv)
{
	const char *name = argv[0];
	int flags = 0, ret = 0;
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
					flags |= FLAG_FORCE;
					break;
				case 'R':
				case 'r':
					flags |= FLAG_RECURSIVE;
					break;
				case 'v':
					flags |= FLAG_VERBOSE;
					break;
				case 'a':
					flags |= FLAG_RECURSIVE;
					flags |= FLAG_PRESERVE_ATTR;
					flags |= FLAG_PRESERVE_MODE;
					break;
				case 'p':
					flags |= FLAG_PRESERVE_MODE;
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
		follow = flags & FLAG_RECURSIVE ? 'P' : 'L';

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
		return cp(argv[0], dst, flags, 0);

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
		ret |= cp(src, buf, flags, 0);
	}

	return ret;
}