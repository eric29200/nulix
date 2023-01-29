#include <fs/fs.h>
#include <proc/sched.h>
#include <mm/mm.h>
#include <stdio.h>
#include <stderr.h>
#include <fcntl.h>
#include <string.h>

/* global inode table */
struct inode_t *inode_table = NULL;
static int inode_htable_bits = 0;
static struct htable_link_t **inode_htable = NULL;

/* inodes lists */
static struct list_head_t free_inodes;
static struct list_head_t used_inodes;

/*
 * Insert an inode in hash table.
 */
void insert_inode_hash(struct inode_t *inode)
{
	htable_insert(inode_htable, &inode->i_htable, inode->i_ino, inode_htable_bits);
}

/*
 * Clear an inode.
 */
void clear_inode(struct inode_t *inode)
{
	/* truncate inode pages */
	truncate_inode_pages(inode, 0);

	/* clear inode */
	list_del(&inode->i_list);
	htable_delete(&inode->i_htable);
	memset(inode, 0, sizeof(struct inode_t));

	/* put it in free list */
	list_add(&inode->i_list, &free_inodes);
}

/*
 * Get an empty inode.
 */
struct inode_t *get_empty_inode(struct super_block_t *sb)
{
	struct list_head_t *pos;
	struct inode_t *inode;

	/* try to get a free inode */
	if (!list_empty(&free_inodes)) {
		inode = list_first_entry(&free_inodes, struct inode_t, i_list);
		goto found;
	}

	/* if no free inodes, try to grab a used one */
	if (list_empty(&free_inodes)) {
		list_for_each(pos, &used_inodes) {
			inode = list_entry(pos, struct inode_t, i_list);
			if (!inode->i_ref) {
				clear_inode(inode);
				goto found;
			}
		}
	}

	return NULL;
found:
	/* set inode */
	inode->i_sb = sb;
	inode->i_ref = 1;
	INIT_LIST_HEAD(&inode->i_pages);
	INIT_LIST_HEAD(&inode->i_mmap);

	/* put inode at the end of LRU list */
	list_del(&inode->i_list);
	list_add_tail(&inode->i_list, &used_inodes);

	return inode;
}

/*
 * Find an inode in hash table.
 */
struct inode_t *find_inode(struct super_block_t *sb, ino_t ino)
{
	struct htable_link_t *node;
	struct inode_t *inode;

	/* try to find inode in cache */
	node = htable_lookup(inode_htable, ino, inode_htable_bits);
	while (node) {
		inode = htable_entry(node, struct inode_t, i_htable);
		if (inode->i_ino == ino && inode->i_sb == sb) {
			inode->i_ref++;
			return inode;
		}

		node = node->next;
	}

	return NULL;
}

/*
 * Get an inode.
 */
struct inode_t *iget(struct super_block_t *sb, ino_t ino)
{
	struct inode_t *inode, *tmp;
	int ret;

	/* try to find inode in table */
	inode = find_inode(sb, ino);
	if (inode) {
		/* cross mount point */
		if (inode->i_mount) {
			tmp = inode->i_mount;
			tmp->i_ref++;
			iput(inode);
			inode = tmp;
		}

		return inode;
	}

	/* read inode not implemented */
	if (!sb->s_op->read_inode)
		return NULL;

	/* get an empty inode */
	inode = get_empty_inode(sb);
	if (!inode)
		return NULL;

	/* read inode */
	inode->i_ino = ino;
	ret = sb->s_op->read_inode(inode);
	if (ret) {
		clear_inode(inode);
		return NULL;
	}

	/* hash the new inode */
	insert_inode_hash(inode);

	return inode;
}

/*
 * Synchronize inode on disk.
 */
static void sync_inode(struct inode_t *inode)
{
	/* write inode if needed */
	if (inode->i_dirt && inode->i_sb) {
		inode->i_sb->s_op->write_inode(inode);
		inode->i_dirt = 0;
	}
}

/*
 * Release an inode.
 */
void iput(struct inode_t *inode)
{
	if (!inode)
		return;

	/* update inode reference count */
	inode->i_ref--;

	/* put inode */
	if (inode->i_sb && inode->i_sb->s_op->put_inode) {
		inode->i_sb->s_op->put_inode(inode);
		if (!inode->i_nlinks)
			return;
	}

	/* sync inode */
	sync_inode(inode);
}

/*
 * Init inodes.
 */
int iinit()
{
	void *addr;
	int nr, i;

	inode_htable_bits = blksize_bits(NR_INODE);

	/* allocate inodes */
	nr = 1 + NR_INODE * sizeof(struct inode_t) / PAGE_SIZE;
	for (i = 0; i < nr; i++) {
		/* get a free page */
		addr = get_free_page();
		if (!addr)
			return -ENOMEM;

		/* reset page */
		memset(addr, 0, PAGE_SIZE);

		/* set inode table */
		if (i == 0)
			inode_table = addr;
	}

	/* allocate inode hash table */
	nr = 1 + NR_INODE * sizeof(struct htable_link_t *) / PAGE_SIZE;
	for (i = 0; i < nr; i++) {
		/* get a free page */
		addr = get_free_page();
		if (!addr)
			return -ENOMEM;

		/* reset page */
		memset(addr, 0, PAGE_SIZE);

		/* set inode hash table */
		if (i == 0)
			inode_htable = addr;
	}

	/* add all inodes to free list */
	INIT_LIST_HEAD(&free_inodes);
	INIT_LIST_HEAD(&used_inodes);
	for (i = 0; i < NR_INODE; i++)
		list_add(&inode_table[i].i_list, &free_inodes);

	/* init inode hash table */
	htable_init(inode_htable, inode_htable_bits);

	return 0;
}
