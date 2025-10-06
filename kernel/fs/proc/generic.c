#include <fs/proc_fs.h>
#include <fcntl.h>
#include <stderr.h>

/*
 * Resolves a path name.
 */
static int xlate_proc_name(const char *name, struct proc_dir_entry **ret, const char **residual)
{
	struct proc_dir_entry *de = &proc_root;
	const char *cp = name, *next;
	size_t len;

	for (;;) {
		/* find next component path */
		next = strchr(cp, '/');
		if (!next)
			break;

		/* find proc entry */
		len = next - cp;
		for (de = de->subdir; de ; de = de->next)
			if (proc_match(cp, len, de))
				break;

		if (!de)
			return -ENOENT;

		cp += len + 1;
	}

	*residual = cp;
	*ret = de;
	return 0;
}

/*
 * Create a proc entry.
 */
struct proc_dir_entry *create_proc_entry(const char *name, mode_t mode, struct proc_dir_entry *parent)
{
	struct proc_dir_entry *de;
	const char *fn = name;
	int len;

	/* resolve path */
	if (!parent && xlate_proc_name(name, &parent, &fn) != 0)
		return NULL;

	/* allocate a new entry */
	len = strlen(fn);
	de = kmalloc(sizeof(struct proc_dir_entry) + len + 1);
	if (!de)
		return NULL;

	/* set entry */
	memset(de, 0, sizeof(struct proc_dir_entry));
	memcpy(((char *) de) + sizeof(struct proc_dir_entry), fn, len + 1);
	de->name = ((char *) de) + sizeof(struct proc_dir_entry);
	de->name_len = len;

	if (S_ISDIR(mode)) {
		if ((mode & S_IALLUGO) == 0)
			mode |= S_IRUGO | S_IXUGO;
		de->ops = &proc_dyna_dir_iops;
		de->nlink = 2;
		de->mode = mode;
	} else {
		if ((mode & S_IFMT) == 0)
			mode |= S_IFREG;
		if ((mode & S_IALLUGO) == 0)
			mode |= S_IRUGO;
		de->nlink = 1;
		de->mode = mode;
	}

	/* register entry */
	proc_register(parent, de);

	return de;
}

/*
 * Generic proc file read.
 */
static int proc_file_read(struct file *filp, char *buf, size_t count, off_t *ppos)
{
	struct inode *inode = filp->f_dentry->d_inode;
	struct proc_dir_entry *de;
	size_t len;
	char *page;

	/* get proc entry */
	de = (struct proc_dir_entry *) inode->u.generic_i;
	if (!de->get_info)
		return 0;

	/* allocate a page */
	page = (char *) get_free_page();
	if (!page)
		return -ENOMEM;

	/* get informations */
	len = de->get_info(page);

	/* file position after end */
	if (*ppos >= len) {
		count = 0;
		goto out;
	}

	/* update count */
	if (*ppos + count > len)
		count = len - *ppos;

	/* copy content to user buffer and update file position */
	memcpy(buf, page + *ppos, count);
	*ppos += count;
out:
	free_page(page);
	return count;
}

/*
 * File operations.
 */
static struct file_operations proc_file_fops = {
	.read			= proc_file_read
};

/*
 * File inode operations.
 */
struct inode_operations proc_file_iops = {
	.fops			= &proc_file_fops,
};