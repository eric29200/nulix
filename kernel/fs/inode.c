#include <fs/fs.h>
#include <proc/sched.h>
#include <mm/mm.h>
#include <stdio.h>
#include <stderr.h>
#include <fcntl.h>
#include <string.h>

#define HASH_BITS		8
#define HASH_SIZE		(1 << HASH_BITS)
#define HASH_MASK		(HASH_SIZE - 1)
#define NR_IHASH		512

/* inodes lists */
static LIST_HEAD(free_inodes);
static LIST_HEAD(used_inodes);
static int nr_inodes = 0;

/* global inode table */
static struct list_head inode_hashtable[HASH_SIZE];

/*
 * Hash an inode.
 */
static uint32_t hash(struct super_block *sb, ino_t i_ino)
{
	uint32_t tmp = i_ino | (uint32_t) sb;
	tmp = tmp + (tmp >> HASH_BITS);
	return tmp & HASH_MASK;
}

/*
 * Insert an inode in hash table.
 */
void insert_inode_hash(struct inode *inode)
{
	struct list_head *head = inode_hashtable + hash(inode->i_sb, inode->i_ino);
	list_add(&inode->i_hash, head);
}

/*
 * Clear an inode.
 */
void clear_inode(struct inode *inode)
{
	/* truncate inode pages */
	truncate_inode_pages(inode, 0);

	/* clear inode */
	list_del(&inode->i_list);
	list_del(&inode->i_hash);

	/* put it in free list */
	list_add(&inode->i_list, &free_inodes);
}

/*
 * Grow inodes.
 */
static int grow_inodes()
{
	struct inode *inodes;
	int i, n;

	/* get some memory */
	inodes = (struct inode *) get_free_page(GFP_KERNEL);
	if (!inodes)
		return -ENOMEM;
	memset(inodes, 0, PAGE_SIZE);

	/* update number of inodes */
	n = PAGE_SIZE / sizeof(struct inode);
	nr_inodes += n;

	/* add inodes to free list */
	for (i = 0; i < n; i++)
		list_add(&inodes[i].i_list, &free_inodes);

	return 0;
}

/*
 * Get an empty inode.
 */
struct inode *get_empty_inode(struct super_block *sb)
{
	struct list_head *pos;
	struct inode *inode;

	/* grow inodes if needed */
	if (list_empty(&free_inodes) && nr_inodes < NR_INODE)
		grow_inodes();

	/* try to get a free inode */
	if (!list_empty(&free_inodes)) {
		inode = list_first_entry(&free_inodes, struct inode, i_list);
		goto found;
	}

	/* if no free inodes, try to grab a used one */
	if (list_empty(&free_inodes)) {
		list_for_each(pos, &used_inodes) {
			inode = list_entry(pos, struct inode, i_list);
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
	INIT_LIST_HEAD(&inode->i_hash);

	/* put inode at the end of LRU list */
	list_del(&inode->i_list);
	list_add_tail(&inode->i_list, &used_inodes);

	return inode;
}

/*
 * Find an inode in hash table.
 */
struct inode *find_inode(struct super_block *sb, ino_t ino)
{
	struct list_head *head, *pos;
	struct inode *inode;

	/* get hash table entry */
	head = inode_hashtable + hash(sb, ino);

	/* find inode */
	list_for_each(pos, head) {
		inode = (struct inode *) list_entry(pos, struct inode, i_hash);
		if (inode->i_ino == ino && inode->i_sb == sb) {
			inode->i_ref++;
			return inode;
		}
	}

	return NULL;
}

/*
 * Get an inode.
 */
struct inode *iget(struct super_block *sb, ino_t ino)
{
	struct inode *inode, *tmp;
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
static void sync_inode(struct inode *inode)
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
void iput(struct inode *inode)
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
 * Check if a file system can be unmounted.
 */
int fs_may_umount(struct super_block *sb)
{
	struct list_head *pos;
	struct inode *inode;

	list_for_each(pos, &used_inodes) {
		inode = list_entry(pos, struct inode, i_list);

		if (inode->i_sb != sb || !inode->i_ref)
			continue;

		if (inode == sb->s_root_inode && inode->i_ref == (inode->i_mount != inode ? 1 : 2))
			continue;

		return 0;
	}

	return 1;
}

/*
 * Init inodes.
 */
int init_inode()
{
	int i;

	/* init hash table */
	for (i = 0; i < HASH_SIZE; i++)
		INIT_LIST_HEAD(&inode_hashtable[i]);

	return 0;
}
