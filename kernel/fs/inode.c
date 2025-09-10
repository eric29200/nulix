#include <fs/fs.h>
#include <proc/sched.h>
#include <mm/mm.h>
#include <stdio.h>
#include <stderr.h>
#include <fcntl.h>
#include <string.h>

#define HASH_BITS			12
#define HASH_SIZE			(1 << HASH_BITS)

/* inodes lists */
static LIST_HEAD(free_inodes);
static LIST_HEAD(used_inodes);
static int nr_inodes = 0;

/* inode hash table */
static struct inode *inode_hash_table[HASH_SIZE];

/*
 * Mark an inode dirty.
 */
void mark_inode_dirty(struct inode *inode)
{
	set_bit(&inode->i_state, I_DIRTY);
}

/*
 * Hash an inode.
 */
static inline int __inode_hashfn(dev_t dev, ino_t ino)
{
	return (dev ^ ino) % HASH_SIZE;
}

/*
 * Get hash bucket.
 */
static inline struct inode **inode_hash(dev_t dev, ino_t ino)
{
	return inode_hash_table + __inode_hashfn(dev, ino);
}

/*
 * Insert an inode in hash table.
 */
void add_to_inode_cache(struct inode *inode)
{
	struct inode **i;

	i = inode_hash(inode->i_sb->s_dev, inode->i_ino);
	inode->i_prev_hash = NULL;
	inode->i_next_hash = *i;

	if (*i)
		inode->i_next_hash->i_prev_hash = inode;

	/* update head */
	*i = inode;
}

/*
 * Remove an inode from cache.
 */
static void remove_from_inode_cache(struct inode *inode)
{
	struct inode *next_hash = inode->i_next_hash, *prev_hash = inode->i_prev_hash;
	struct inode **i;

	/* unlink this inode */
	inode->i_next_hash = NULL;
	inode->i_prev_hash = NULL;

	/* update next */
	if (next_hash)
		next_hash->i_prev_hash = prev_hash;

	/* update previous */
	if (prev_hash)
		prev_hash->i_next_hash = next_hash;

	/* update head */
	i = inode_hash(inode->i_sb->s_dev, inode->i_ino);
	if (*i == inode)
		*i = next_hash;
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
	remove_from_inode_cache(inode);
	memset(inode, 0, sizeof(struct inode));

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
	inodes = (struct inode *) get_free_page();
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
			if (!inode->i_count) {
				clear_inode(inode);
				goto found;
			}
		}
	}

	return NULL;
found:
	/* set inode */
	inode->i_sb = sb;
	inode->i_count = 1;
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
struct inode *find_inode(struct super_block *sb, ino_t ino)
{
	struct inode *inode;

	for (inode = *inode_hash(sb->s_dev, ino); inode != NULL; inode = inode->i_next_hash) {
		if (inode->i_ino == ino && inode->i_sb == sb) {
			inode->i_count++;
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
	struct inode *inode;
	int ret;

	/* try to find inode in table */
	inode = find_inode(sb, ino);
	if (inode)
		return inode;

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
	add_to_inode_cache(inode);

	return inode;
}

/*
 * Write inode on disk.
 */
static void write_inode(struct inode *inode)
{
	/* write inode */
	if (inode->i_sb && inode->i_sb->s_op && inode->i_sb->s_op->write_inode)
		inode->i_sb->s_op->write_inode(inode);

	/* clear dirty state */
	clear_bit(&inode->i_state, I_DIRTY);
}

/*
 * Synchronize inodes on disk.
 */
void sync_inodes(dev_t dev)
{
	struct list_head *pos;
	struct inode *inode;

	list_for_each(pos, &used_inodes) {
		inode = list_entry(pos, struct inode, i_list);

		/* clean inode */
		if (!test_bit(&inode->i_state, I_DIRTY) || (dev && inode->i_sb->s_dev != dev))
			continue;

		/* write inode */
		write_inode(inode);
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
	inode->i_count--;

	/* put inode */
	if (inode->i_sb && inode->i_sb->s_op->put_inode) {
		inode->i_sb->s_op->put_inode(inode);
		if (!inode->i_nlinks)
			return;
	}

	/* write inode if needed */
	if (test_bit(&inode->i_state, I_DIRTY))
		write_inode(inode);
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

		if (inode->i_sb != sb || !inode->i_count)
			continue;

		if (inode == sb->s_root->d_inode)
			continue;

		return 0;
	}

	return 1;
}

/*
 * Init inodes.
 */
void init_inode()
{
	int i;

	/* init hash table */
	for (i = 0; i < HASH_SIZE; i++)
		inode_hash_table[i] = NULL;
}
