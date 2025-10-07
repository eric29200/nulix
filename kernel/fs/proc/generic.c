#include <fs/proc_fs.h>
#include <fcntl.h>
#include <stderr.h>

/* dynamic inodes bitmap */
static uint8_t proc_alloc_map[PROC_NDYNAMIC / 8] = { 0 };

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
 * Create a dynamic inode number.
 */
static int make_inode_number()
{
	int i;

	/* find first free inode */
	i = find_first_zero_bit((uint32_t *) proc_alloc_map, PROC_NDYNAMIC);
	if (i < 0 || i >= PROC_NDYNAMIC) 
		return -1;

	/* set inode */
	set_bit((uint32_t *) proc_alloc_map, i);
	return PROC_DYNAMIC_FIRST + i;
}

/*
 * Register a proc filesystem entry.
 */
int proc_register(struct proc_dir_entry *dir, struct proc_dir_entry *de)
{
	int i;

	/* create an inode number if needed */
	if (de->low_ino == 0) {
		i = make_inode_number();
		if (i < 0)
			return -EAGAIN;
		de->low_ino = i;
	}

	/* set parent */
	de->parent = dir;

	/* add entry to parent */
	de->next = dir->subdir;
	dir->subdir = de;

	/* set default operations */
	if (S_ISDIR(de->mode)) {
		if (!de->ops)
			de->ops = &proc_dir_iops;
		dir->nlink++;
	} else if (S_ISLNK(de->mode) && !de->ops) {
		de->ops = &proc_link_iops;
	} else if (!de->ops) {
			de->ops = &proc_file_iops;
	}

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
		de->ops = &proc_dir_iops;
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
 * Create a read entry.
 */
struct proc_dir_entry *create_proc_read_entry(const char *name, mode_t mode, struct proc_dir_entry *dir, read_proc_t *read_proc)
{
	struct proc_dir_entry *de;

	/* create entry */
	de = create_proc_entry(name, mode, dir);
	if (!de)
		return NULL;

	/* set read proc */
	de->read_proc = read_proc;
	return de;
}

/*
 * Create a directory entry.
 */
struct proc_dir_entry *proc_mkdir_mode(const char *name, mode_t mode, struct proc_dir_entry *dir)
{
	struct proc_dir_entry *de;

	/* create entry */
	de = create_proc_entry(name, S_IFDIR | mode, dir);
	if (!de)
		return NULL;

	/* set inode operations */
	de->ops = &proc_dir_iops;
	return de;
}

/*
 * Create a directory entry.
 */
struct proc_dir_entry *proc_mkdir(const char *name, struct proc_dir_entry *dir)
{
	return proc_mkdir_mode(name, S_IRUGO | S_IXUGO, dir);
}

/*
 * Generic proc file read.
 */
static int proc_file_read(struct file *filp, char *buf, size_t count, off_t *ppos)
{
	struct inode *inode = filp->f_dentry->d_inode;
	int eof = 0, len, n, ret = 0;
	struct proc_dir_entry *de;
	char *page, *start;

	/* get proc entry */
	de = (struct proc_dir_entry *) inode->u.generic_i;
	if (!de->read_proc)
		return 0;

	/* allocate a page */
	page = (char *) get_free_page();
	if (!page)
		return -ENOMEM;

	while (count > 0 && !eof) {
		/* limit to page size */
		len = count >= PAGE_SIZE ? PAGE_SIZE : count;

		/* read proc entry */
		n = de->read_proc(page, &start, *ppos, len, &eof);

		/* end of file */
		if (n == 0)
			break;

		/* handle error */
		if (n < 0) {
			if (ret == 0)
				ret = n;
			break;
		}

		/* copy data */
		memcpy(buf, start, n);

		/* update position */
		*ppos += n;
		count -= n;
		buf += n;
		ret += n;
	}

	/* free page */
	free_page(page);

	return ret;
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

/*
 * Generic procfs read directory.
 */
int proc_readdir(struct file *filp, void *dirent, filldir_t filldir)
{
	struct inode *inode = filp->f_dentry->d_inode;
	struct proc_dir_entry *de;
	ino_t ino = inode->i_ino;
	size_t i = filp->f_pos;

	/* get proc entry */
	de = (struct proc_dir_entry *) inode->u.generic_i;

	/* "." entry */
	if (i == 0) {
		if (filldir(dirent, ".", 1, i, inode->i_ino))
			return 0;
		i++;
		filp->f_pos++;
	}

	/* ".." entry */
	if (i == 1) {
		if (filldir(dirent, "..", 2, i, de->parent->low_ino))
			return 0;
		i++;
		filp->f_pos++;
	}

	/* skip first entries */
	ino &= ~0xFFFF;
	de = de->subdir;
	i -= 2;
	for (;;) {
		if (!de)
			return 1;
		if (!i)
			break;
		de = de->next;
		i--;
	}

	/* read entries */
	do {
		if (filldir(dirent, de->name, de->name_len, filp->f_pos, ino | de->low_ino))
			return 0;
		filp->f_pos++;
		de = de->next;
	} while (de);

	return 0;
}

/*
 * Generic procfs read directory.
 */
struct dentry *proc_lookup(struct inode *dir, struct dentry *dentry)
{
	struct inode *inode = NULL;
	struct proc_dir_entry *de;
	int ret = -ENOENT;
	ino_t ino;

	/* get proc entry */
	de = (struct proc_dir_entry *) dir->u.generic_i;

	/* find matching entry */
	if (de) {
		for (de = de->subdir; de != NULL; de = de->next) {
			if (proc_match(dentry->d_name.name, dentry->d_name.len, de)) {
				ino = de->low_ino | (dir->i_ino & ~(0xFFFF));
				ret = -EINVAL;
				inode = proc_get_inode(dir->i_sb, ino, de);
				break;
			}
		}
	}

	/* found inode */
	if (inode) {
		d_add(dentry, inode);
		return NULL;
	}

	return ERR_PTR(ret);
}

/*
 * Directory file operations.
 */
static struct file_operations proc_dir_fops = {
	.readdir		= proc_readdir,
};

/*
 * Directory inode operations.
 */
struct inode_operations proc_dir_iops = {
	.fops			= &proc_dir_fops,
	.lookup			= proc_lookup,
};