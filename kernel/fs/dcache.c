#include <fs/fs.h>
#include <stdio.h>

#define switch(x, y) 		do {					\
					__typeof__ (x) __tmp = x;	\
					x = y;				\
					y = __tmp;			\
				} while (0)

/*
 * Release a dentry.
 */
void dput(struct dentry *dentry)
{
	if (!dentry)
		return;

	/* update reference count */
	dentry->d_count--;
	if (dentry->d_count < 0)
		panic("dput: negative d_count");

	/* release inode */
	iput(dentry->d_inode);
}

/*
 * Instantiate a dentry.
 */
void d_instantiate(struct dentry *dentry, struct inode *inode)
{
	dentry->d_inode = inode;
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