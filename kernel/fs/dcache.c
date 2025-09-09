#include <fs/fs.h>
#include <stdio.h>

#define switch(x, y) 		do {					\
					__typeof__ (x) __tmp = x;	\
					x = y;				\
					y = __tmp;			\
				} while (0)

/*
 * Get a dentry.
 */
struct dentry *dget(struct dentry *dentry)
{
	if (dentry)
		dentry->d_count++;

	return dentry;
}

/*
 * Release a dentry.
 */
void dput(struct dentry *dentry)
{
	if (dentry)
		dentry->d_count--;
}

/*
 * Allocate a dentry.
 */
struct dentry *d_alloc(struct dentry *parent, const struct qstr *name)
{
	struct dentry *dentry;
	char *str;

	/* allocate a new dentry */
	dentry = kmalloc(sizeof(struct dentry));
	if (!dentry)
		return NULL;

	/* allocate name */
	str = kmalloc(name->len + 1);
	if (!str) {
		kfree(dentry);
		return NULL;
	}

	/* set name */
	memcpy(str, name->name, name->len);
	str[name->len] = 0;

	/* set dentry */
	dentry->d_count = 0;
	dentry->d_inode = NULL;
	dentry->d_parent = parent;
	dentry->d_mounts = dentry;
	dentry->d_covers = dentry;
	dentry->d_name.name = str;
	dentry->d_name.len = name->len;
	dentry->d_name.hash = name->hash;

	return dentry;
}

/*
 * Allocate a root dentry.
 */
struct dentry *d_alloc_root(struct inode *root_inode)
{
	struct dentry *dentry;

	/* allocate a new dentry */
	dentry = d_alloc(NULL, &(const struct qstr) { "/", 1, 0 });
	if (!dentry)
		return NULL;

	/* set dentry */
	dentry->d_count = 1;
	dentry->d_parent = dentry;

	/* instantiate dentry */
	d_instantiate(dentry, root_inode);

	return dentry;
}

/*
 * Free a dentry.
 */
void d_free(struct dentry *dentry)
{
	kfree(dentry->d_name.name);
	kfree(dentry);
}

/*
 * Instantiate a dentry.
 */
void d_instantiate(struct dentry *dentry, struct inode *inode)
{
	dentry->d_inode = inode;
}

/*
 * Add a dentry.
 */
void d_add(struct dentry *dentry, struct inode *inode)
{
	d_instantiate(dentry, inode);
}

/*
 * Delete a dentry.
 */
void d_delete(struct dentry *dentry)
{
	struct inode *inode = dentry->d_inode;

	dentry->d_inode = NULL;
	iput(inode);
}

/*
 * Move a dentry.
 */
void d_move(struct dentry *dentry, struct dentry *target)
{
	/* check inode */
	if (!dentry->d_inode)
		panic("d_move: moving negative dcache entry");

	/* switch names */
	switch(dentry->d_name.name, target->d_name.name);
	switch(dentry->d_name.len, target->d_name.len);
	switch(dentry->d_name.hash, target->d_name.hash);

	/* delete target */
	d_delete(target);
}
