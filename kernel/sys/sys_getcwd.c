#include <sys/syscall.h>
#include <mm/mm.h>
#include <fcntl.h>
#include <stderr.h>

/*
 * Find a directory entry by its inode.
 */
static struct dirent64_t *find_dirent(int fd, struct dirent64_t *dirp, ino_t ino)
{
	struct dirent64_t *dirent;
	int entries_size, n;

	for (;;) {
		/* get next directory entries */
		entries_size = sys_getdents64(fd, dirp, PAGE_SIZE);
		if (entries_size <= 0)
			break;

		/* parse directory entries */
		for (dirent = dirp, n = 0; n < entries_size;) {
			/* matching inode */
			if (dirent->d_inode == ino)
				return dirent;

			/* go to next directory entry */
			n += dirent->d_reclen;
			dirent = (struct dirent64_t *) ((char *) dirent + dirent->d_reclen);
		}
	}

	return NULL;
}

/*
 * Prepend directory entry to buffer.
 */
static int preprend_dirent(char *buf, size_t size, size_t max_size, struct dirent64_t *dirent)
{
	size_t dirent_name_len, nb_shift;
	int i;

	/* get entry name length */
	dirent_name_len = strlen(dirent->d_name);

	/* compute number of shifts */
	if (dirent_name_len + 1 >= max_size)
		nb_shift = 0;
	else if (size + dirent_name_len + 1 > max_size)
		nb_shift = max_size - dirent_name_len - 1;
	else
		nb_shift = size;

	/* shift existing buffer */
	for (i = nb_shift - 1; i >= 0; i--)
		buf[i + dirent_name_len + 1] = buf[i];

	/* prepend '/' */
	buf[0] = '/';

	/* limit dirent name length */
	if (dirent_name_len + 1 > max_size)
		dirent_name_len = max_size;

	/* prepend directory entry */
	memcpy(buf + 1, dirent->d_name, dirent_name_len);

	/* return path length */
	return nb_shift + dirent_name_len + 1;
}

/*
 * Get current working dir system call.
 */
int sys_getcwd(char *buf, size_t size)
{
	int parent_fd = -1, fd = -1, n = 0;
	struct dirent64_t *dirp, *dirent;
	struct inode_t *inode;

	/* check size */
	if (size < 2)
		return -EINVAL;

	/* root directory : return "/" */
	if (current_task->fs->cwd == current_task->fs->root) {
		strncpy(buf, "/", 1);
		return 1;
	}

	/* allocate dir buffer */
	dirp = get_free_page();
	if (!dirp)
		return -ENOMEM;

	/* walk up until root directory */
	for (fd = AT_FDCWD, inode = current_task->fs->cwd; inode != current_task->fs->root;) {
		/* open parent directory */
		parent_fd = do_open(fd, "..", O_RDONLY, 0);
		if (parent_fd < 0)
			break;

		/* find directory entry */
		dirent = find_dirent(parent_fd, dirp, inode->i_ino);
		if (!dirent)
			break;

		/* prepend directory entry */
		n = preprend_dirent(buf, n, size - 1, dirent);

		/* close current directory */
		if (fd >= 0)
			sys_close(fd);

		/* go to parent directory */
		fd = parent_fd;
		inode = current_task->files->filp[fd]->f_inode;
	}

	/* end path */
	buf[n] = 0;

	/* close directories */
	if (fd > 0)
		sys_close(fd);
	if (parent_fd > 0)
		sys_close(parent_fd);

	/* free dir buffer */
	free_page(dirp);

	return n;
}
