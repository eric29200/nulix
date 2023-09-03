#include <fs/fs.h>
#include <fs/minix_fs.h>
#include <mm/mm.h>
#include <string.h>
#include <stderr.h>

/*
 * Get directory entries system call.
 */
int minix_getdents64(struct file *filp, void *dirp, size_t count)
{
	struct minix_sb_info *sbi = minix_sb(filp->f_inode->i_sb);
	struct minix3_dir_entry *de3;
	struct dirent64 *dirent;
	int entries_size, ret;
	size_t name_len;
	char *name;
	ino_t ino;
	void *de;

	/* allocate directory entry */
	de = kmalloc(sbi->s_dirsize);
	if (!de)
		return -ENOMEM;

	/* set minix directory entries pointer */
	de3 = de;

	/* walk through all entries */
	for (entries_size = 0, dirent = (struct dirent64 *) dirp;;) {
		/* read minix dir entry */
		if (minix_file_read(filp, (char *) de, sbi->s_dirsize) != sbi->s_dirsize)
			goto out;

		/* get inode number and file name */
		ino = de3->d_inode;
		name = de3->d_name;

		/* skip null entries */
		if (ino == 0)
			continue;

		/* fill in directory entry */
		name_len = strlen(name);
		ret = filldir(dirent, name, name_len, ino, count);
		if (ret) {
			filp->f_pos -= sbi->s_dirsize;
			return entries_size;
		}

		/* go to next entry */
		count -= dirent->d_reclen;
		entries_size += dirent->d_reclen;
		dirent = (struct dirent64 *) ((char *) dirent + dirent->d_reclen);
	}

out:
	kfree(de);
	return entries_size;
}
