#include <fs/fs.h>
#include <proc/sched.h>
#include <mm/mm.h>
#include <stdio.h>
#include <stderr.h>
#include <fcntl.h>
#include <string.h>

/* global inode table */
static struct inode_t inode_table[NR_INODE];

/*
 * Get an empty inode.
 */
struct inode_t *get_empty_inode(struct super_block_t *sb)
{
	struct inode_t *inode;
	int i;

	/* find a free inode */
	for (i = 0; i < NR_INODE; i++) {
		if (!inode_table[i].i_ref) {
			inode = &inode_table[i];
			break;
		}
	}

	/* no more inode */
	if (!inode)
		return NULL;

	/* reset inode */
	memset(inode, 0, sizeof(struct inode_t));
	inode->i_sb = sb;
	inode->i_ref = 1;
	inode->i_mapping.inode = inode;
	INIT_LIST_HEAD(&inode->i_mapping.clean_pages);
	INIT_LIST_HEAD(&inode->i_mapping.dirty_pages);

	return inode;
}

/*
 * Get an inode.
 */
struct inode_t *iget(struct super_block_t *sb, ino_t ino)
{
	struct inode_t *inode, *tmp;
	int i;

	/* try to find inode in table */
	for (i = 0; i < NR_INODE; i++) {
		if (inode_table[i].i_ino == ino && inode_table[i].i_sb == sb) {
			inode = &inode_table[i];
			inode->i_ref++;

			/* cross mount point */
			if (inode->i_mount) {
				tmp = inode->i_mount;
				tmp->i_ref++;
				iput(inode);
				inode = tmp;
			}

			return inode;
		}
	}

	/* get an empty inode */
	inode = get_empty_inode(sb);
	if (!inode)
		return NULL;

	/* read inode */
	inode->i_ino = ino;
	sb->s_op->read_inode(inode);

	return inode;
}

/*
 * Synchronize inode on disk.
 */
static void sync_inode(struct inode_t *inode)
{
	/* sync data */
	filemap_fdatasync(&inode->i_mapping);

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

	/* removed inode : truncate and free it */
	if (!inode->i_nlinks && inode->i_ref == 0 && inode->i_sb) {
		inode->i_sb->s_op->put_inode(inode);
		return;
	}

	/* sync inode */
	sync_inode(inode);

	/* no more references : reset inode */
	if (inode->i_ref == 0)
		memset(inode, 0, sizeof(struct inode_t));
}
