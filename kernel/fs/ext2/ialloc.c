#include <fs/fs.h>
#include <fs/ext2_fs.h>
#include <proc/sched.h>
#include <stdio.h>
#include <stderr.h>
#include <fcntl.h>

/*
 * Read the inodes bitmap of a block group.
 */
static struct buffer_head *ext2_read_inode_bitmap(struct super_block *sb, uint32_t block_group)
{
	struct ext2_group_desc *gdp;

	/* get group descriptor */
	gdp = ext2_get_group_desc(sb, block_group, NULL);
	if (!gdp)
		return NULL;

	/* load inodes bitmap */
	return bread(sb->s_dev, gdp->bg_inode_bitmap, sb->s_blocksize);
}

/*
 * Create a new Ext2 inode.
 */
struct inode *ext2_new_inode(struct inode *dir, mode_t mode)
{
	struct ext2_sb_info *sbi = ext2_sb(dir->i_sb);
	struct buffer_head *gdp_bh, *bitmap_bh;
	struct ext2_group_desc *gdp;
	uint32_t group_no, bgi;
	struct inode *inode;
	int i;

	/* get an empty inode */
	inode = get_empty_inode(dir->i_sb);
	if (!inode)
		return NULL;

	/* try to find a group with free inodes (start with directory block group) */
	group_no = dir->u.ext2_i.i_block_group;
	for (bgi = 0; bgi < sbi->s_groups_count; bgi++, group_no++) {
		/* rewind to first group if needed */
		if (group_no >= sbi->s_groups_count)
			group_no = 0;

		/* get group descriptor */
		gdp = ext2_get_group_desc(inode->i_sb, group_no, &gdp_bh);
		if (!gdp) {
			iput(inode);
			return NULL;
		}

		/* no free inodes in this group */
		if (!gdp->bg_free_inodes_count)
			continue;

		/* get group inodes bitmap */
		bitmap_bh = ext2_read_inode_bitmap(inode->i_sb, group_no);
		if (!bitmap_bh) {
			iput(inode);
			return NULL;
		}

		/* get first free inode in bitmap */
		i = ext2_get_free_bitmap(bitmap_bh);
		if (i != -1 && i < (int) sbi->s_inodes_per_group)
			goto allocated;

		/* release bitmap block */
		brelse(bitmap_bh);
	}

	/* release inode */
	iput(inode);
	return NULL;
allocated:
	/* set inode number */
	inode->i_ino = group_no * sbi->s_inodes_per_group + i + 1;
	if (inode->i_ino < sbi->s_first_ino || inode->i_ino > sbi->s_es->s_inodes_count) {
		brelse(bitmap_bh);
		iput(inode);
		return NULL;
	}

	/* set inode */
	inode->i_mode = mode;
	inode->i_uid = current_task->fsuid;
	inode->i_gid = current_task->fsgid;
	inode->i_atime = inode->i_mtime = inode->i_ctime = CURRENT_TIME;
	inode->i_size = 0;
	inode->i_blocks = 0;
	inode->i_nlinks = 1;
	inode->i_op = NULL;
	inode->i_count = 1;
	inode->u.ext2_i.i_block_group = group_no;
	inode->u.ext2_i.i_flags = dir->u.ext2_i.i_flags;
	inode->u.ext2_i.i_faddr = 0;
	inode->u.ext2_i.i_frag_no = 0;
	inode->u.ext2_i.i_frag_size = 0;
	inode->u.ext2_i.i_file_acl = 0;
	inode->u.ext2_i.i_dir_acl = 0;
	inode->u.ext2_i.i_dtime = 0;
	inode->u.ext2_i.i_generation = dir->u.ext2_i.i_generation;

	/* set block in bitmap */
	EXT2_BITMAP_SET(bitmap_bh, i);

	/* release inodes bitmap */
	mark_buffer_dirty(bitmap_bh);
	brelse(bitmap_bh);

	/* update group descriptor */
	gdp->bg_free_inodes_count = gdp->bg_free_inodes_count - 1;
	if (S_ISDIR(inode->i_mode))
		gdp->bg_used_dirs_count = gdp->bg_used_dirs_count + 1;
	mark_buffer_dirty(gdp_bh);

	/* update super block */
	sbi->s_es->s_free_inodes_count = sbi->s_es->s_free_inodes_count - 1;
	mark_buffer_dirty(sbi->s_sbh);

	/* mark inode dirty */
	mark_inode_dirty(inode);

	return inode;
}

/*
 * Free a Ext2 inode.
 */
int ext2_free_inode(struct inode *inode)
{
	struct buffer_head *bitmap_bh, *gdp_bh;
	struct ext2_group_desc *gdp;
	struct ext2_sb_info *sbi;
	uint32_t block_group, bit;

	/* check inode */
	if (!inode)
		return 0;

	/* check if inode is still referenced */
	if (inode->i_count > 1) {
		printf("[Ext2-fs] Trying to free inode %d with ref=%d\n", inode->i_ino, inode->i_count);
		return -EINVAL;
	}

	/* get super block */
	sbi = ext2_sb(inode->i_sb);

	/* check if inode is not reserved */
	if (inode->i_ino < sbi->s_first_ino || inode->i_ino > sbi->s_es->s_inodes_count) {
		printf("[Ext2-fs] Trying to free inode reserved or non existent inode %d\n", inode->i_ino);
		return -EINVAL;
	}

	/* get block group */
	block_group = (inode->i_ino - 1) / sbi->s_inodes_per_group;
	bit = (inode->i_ino - 1) % sbi->s_inodes_per_group;

	/* get inode bitmap */
	bitmap_bh = ext2_read_inode_bitmap(inode->i_sb, block_group);
	if (!bitmap_bh)
		return -EIO;

	/* clear inode in bitmap */
	EXT2_BITMAP_CLR(bitmap_bh, bit);
	mark_buffer_dirty(bitmap_bh);
	brelse(bitmap_bh);

	/* update group descriptor */
	gdp = ext2_get_group_desc(inode->i_sb, block_group, &gdp_bh);
	gdp->bg_free_inodes_count = gdp->bg_free_inodes_count + 1;
	if (S_ISDIR(inode->i_mode))
		gdp->bg_used_dirs_count = gdp->bg_used_dirs_count - 1;
	mark_buffer_dirty(gdp_bh);

	/* update super block */
	sbi->s_es->s_free_inodes_count = sbi->s_es->s_free_inodes_count + 1;
	mark_buffer_dirty(sbi->s_sbh);

	/* clear inode */
	clear_inode(inode);

	return 0;
}
