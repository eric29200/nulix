#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <utime.h>
#include <dirent.h>
#include <sys/stat.h>

#include "build_path.h"
#include "copy.h"

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
	if ((flags & FLAG_CP_FORCE) && unlink(dst) < 0 && errno != ENOENT) {
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
	if ((flags & FLAG_CP_FORCE) && unlink(dst) < 0 && errno != ENOENT) {
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
	if (dfd < 0 && (flags & FLAG_CP_FORCE)) {
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
			dn = write(dfd, bp, sn);
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
static int cp_dir(const char *src, const char *dst, int flags, int follow, int level, const struct stat *src_statbuf)
{
	char src_path[PATH_MAX], dst_path[PATH_MAX];
	struct dirent *entry;
	int ret = 0;
	DIR *dirp;

	/* check recursive flag */
	if (!(flags & FLAG_CP_RECURSIVE)) {
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
		ret |= copy(src_path, dst_path, flags, follow, level + 1);
	}

	/* close directory */
	closedir(dirp);

	return ret;
}

/*
 * Copy.
 */
int copy(const char *src, const char *dst, int flags, int follow, int level)
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
	if (flags & FLAG_CP_VERBOSE)
		printf("'%s' -> '%s'\n", src, dst);

	/* copy */
	if (S_ISLNK(st.st_mode))
		ret = cp_link(src, dst, flags);
	else if (S_ISDIR(st.st_mode))
		ret = cp_dir(src, dst, flags, follow, level, &st);
	else if ((flags & FLAG_CP_PRESERVE_ATTR) && (S_ISBLK(st.st_mode) || S_ISCHR(st.st_mode) || S_ISSOCK(st.st_mode) || S_ISFIFO(st.st_mode)))
		ret = cp_nod(&st, dst, flags);
	else
		ret = cp_file(src, dst, flags, &st);

	/* on error return */
	if (ret)
		return ret;

	/* preserve attributes */
	if ((flags & FLAG_CP_PRESERVE_ATTR) || (flags & FLAG_CP_PRESERVE_MODE)) {
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
