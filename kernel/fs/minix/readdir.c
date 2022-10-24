#include <fs/fs.h>
#include <fs/minix_fs.h>
#include <mm/mm.h>
#include <string.h>
#include <stderr.h>

/*
 * Get directory entries system call.
 */
int minix_getdents64(struct file_t *filp, void *dirp, size_t count)
{
	struct minix_sb_info_t *sbi = minix_sb(filp->f_inode->i_sb);
	struct minix3_dir_entry_t *de3;
	struct dirent64_t *dirent;
	int entries_size;
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
	for (entries_size = 0, dirent = (struct dirent64_t *) dirp;;) {
		/* read minix dir entry */
		if (minix_file_read(filp, (char *) de, sbi->s_dirsize) != sbi->s_dirsize)
			goto out;

		/* get inode number and file name */
		ino = de3->d_inode;
		name = de3->d_name;

		/* skip null entries */
		if (ino == 0)
			continue;

		/* not enough space to fill in next dir entry : break */
		name_len = strlen(name);
		if (count < sizeof(struct dirent64_t) + name_len + 1) {
			filp->f_pos -= sbi->s_dirsize;
			goto out;
		}

		/* fill in dirent */
		dirent->d_inode = ino;
		dirent->d_off = 0;
		dirent->d_reclen = sizeof(struct dirent64_t) + name_len + 1;
		dirent->d_type = 0;
		memcpy(dirent->d_name, name, name_len);
		dirent->d_name[name_len] = 0;

		/* go to next entry */
		count -= dirent->d_reclen;
		entries_size += dirent->d_reclen;
		dirent = (struct dirent64_t *) ((char *) dirent + dirent->d_reclen);
	}

out:
	kfree(de);
	return entries_size;
}
