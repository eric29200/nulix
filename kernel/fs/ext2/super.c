#include <fs/fs.h>
#include <fs/ext2_fs.h>
#include <mm/mm.h>
#include <stdio.h>
#include <stderr.h>

/*
 * Read a Ext2 super block.
 */
static int ext2_read_super(struct super_block_t *sb, void *data, int flags)
{
	uint32_t block, sb_block = 1, offset = 0, logic_sb_block = 1;
	int err = -ENOSPC, blocksize;
	struct ext2_sb_info_t *sbi;
	uint32_t i;

	/* unused data/flags */
	UNUSED(data);
	UNUSED(flags);

	/* allocate Ext2 in memory super block */
	sb->s_fs_info = sbi = (struct ext2_sb_info_t *) kmalloc(sizeof(struct ext2_sb_info_t));
	if (!sbi)
		return -ENOMEM;

	/* set default block size */
	blocksize = EXT2_BLOCK_SIZE;
	sb->s_blocksize = EXT2_BLOCK_SIZE;
	sb->s_blocksize_bits = EXT2_BLOCK_SIZE_BITS;

	/* read first block = super block */
	sbi->s_sbh = bread(sb, sb_block);
	if (!sbi->s_sbh) {
		err = -EIO;
		goto err_bad_sb;
	}

	/* set super block */
	sbi->s_es = (struct ext2_super_block_t *) sbi->s_sbh->b_data;
	sb->s_magic = le16toh(sbi->s_es->s_magic);
	sb->s_root_inode = NULL;
	sb->s_op = &ext2_sops;

	/* check magic number */
	if (sb->s_magic != EXT2_MAGIC)
		goto err_bad_magic;

	/* check revision level */
	if (le32toh(sbi->s_es->s_rev_level) > EXT2_DYNAMIC_REV)
		goto err_bad_rev;

	/* check block size */
	blocksize = EXT2_BLOCK_SIZE << le32toh(sbi->s_es->s_log_block_size);
	if (blocksize != 1024 && blocksize != 2048 && blocksize != 4096)
		goto err_bad_blocksize;

	/* specific block size : reread super blockk */
	if (blocksize != EXT2_BLOCK_SIZE) {
		/* release super block buffer */
		brelse(sbi->s_sbh);

		/* set new block size */
		sb->s_blocksize = blocksize;
		sb->s_blocksize_bits = blksize_bits(blocksize);

		/* compute super block offset */
		logic_sb_block = (sb_block * EXT2_BLOCK_SIZE) / sb->s_blocksize;
		offset = (sb_block * EXT2_BLOCK_SIZE) % sb->s_blocksize;

		/* reread super block */
		sbi->s_sbh = bread(sb, logic_sb_block);
		if (!sbi->s_sbh) {
			err = -EIO;
			goto err_bad_sb;
		}

		/* set super block */
		sbi->s_es = (struct ext2_super_block_t *) (sbi->s_sbh->b_data + offset);
		sb->s_magic = le16toh(sbi->s_es->s_magic);

		/* check magic number */
		if (sb->s_magic != EXT2_MAGIC)
			goto err_bad_magic;

		/* check revision level */
		if (le32toh(sbi->s_es->s_rev_level) > EXT2_DYNAMIC_REV)
			goto err_bad_rev;
	}

	/* get inode size */
	if (le32toh(sbi->s_es->s_rev_level) == EXT2_GOOD_OLD_REV) {
		sbi->s_inode_size = EXT2_GOOD_OLD_INODE_SIZE;
		sbi->s_first_ino = EXT2_GOOD_OLD_FIRST_INO;
	} else {
		sbi->s_inode_size = le16toh(sbi->s_es->s_inode_size);
		sbi->s_first_ino = le32toh(sbi->s_es->s_first_ino);
	}

	/* set super block */
	sbi->s_inodes_per_block = sb->s_blocksize / EXT2_INODE_SIZE(sb);
	sbi->s_blocks_per_group = le32toh(sbi->s_es->s_blocks_per_group);
	sbi->s_inodes_per_group = le32toh(sbi->s_es->s_inodes_per_group);
	sbi->s_itb_per_group = sbi->s_inodes_per_group / sbi->s_inodes_per_block;
	sbi->s_desc_per_block = sb->s_blocksize / sizeof(struct ext2_group_desc_t);
	sbi->s_groups_count = (le32toh(sbi->s_es->s_blocks_count) - le32toh(sbi->s_es->s_first_data_block)
			       + sbi->s_blocks_per_group - 1) / sbi->s_blocks_per_group;
	sbi->s_gdb_count = (sbi->s_groups_count + sbi->s_desc_per_block - 1) / sbi->s_desc_per_block;

	/* allocate group descriptors buffers */
	sbi->s_group_desc = (struct buffer_head_t **) kmalloc(sizeof(struct buffer_head_t *) * sbi->s_gdb_count);
	if (!sbi->s_group_desc) {
		err = -ENOMEM;
		goto err_no_gdb;
	}

	/* reset group descriptors buffers */
	for (i = 0; i < sbi->s_gdb_count; i++)
		sbi->s_group_desc[i] = NULL;

	/* read group descriptors */
	for (i = 0; i < sbi->s_gdb_count; i++) {
		/* get group descriptor block = +1 for super block stored in front of each group */
		block = logic_sb_block + i + 1;

		/* read group descriptor */
		sbi->s_group_desc[i] = bread(sb, block);
		if (!sbi->s_group_desc[i]) {
			err = -EIO;
			goto err_read_gdb;
		}
	}

	/* get root inode */
	sb->s_root_inode = iget(sb, EXT2_ROOT_INO);
	if (!sb->s_root_inode)
		goto err_root_inode;

	return 0;
err_root_inode:
	printf("[Ext2-fs] Can't get root inode\n");
	goto err_release_gdb;
err_read_gdb:
	printf("[Ext2-fs] Can't read group descriptors\n");
	goto err_release_gdb;
err_no_gdb:
	printf("[Ext2-fs] Can't allocate group descriptors\n");
err_release_gdb:
	for (i = 0; i < sbi->s_gdb_count; i++)
		brelse(sbi->s_group_desc[i]);
	kfree(sbi->s_group_desc);
	goto err_release_sb;
err_bad_blocksize:
	printf("[Ext2-fs] Wrong block size (only 1024, 2048 and 4096 supported)\n");
	goto err_release_sb;
err_bad_rev:
	printf("[Ext2-fs] Wrong revision level\n");
	goto err_release_sb;
err_bad_magic:
	printf("[Ext2-fs] Wrong magic number\n");
err_release_sb:
	brelse(sbi->s_sbh);
	goto err;
err_bad_sb:
	printf("[Ext2-fs] Can't read super block\n");
err:
	kfree(sbi);
	return err;
}

/*
 * Get Ext2 File system status.
 */
static void ext2_statfs(struct super_block_t *sb, struct statfs64_t *buf)
{
	struct ext2_sb_info_t *sbi = ext2_sb(sb);
	uint32_t overhead_per_group, overhead;

	/* compute overhead */
	overhead_per_group = 1			/* super block */
		+ sbi->s_gdb_count		/* descriptors group */
		+ 1				/* blocks bitmap */
		+ 1				/* inodes bitmap */
		+ sbi->s_itb_per_group;		/* inodes table */
	overhead = le32toh(sbi->s_es->s_first_data_block) + sbi->s_groups_count * overhead_per_group;

	/* set stat buffer */
	buf->f_type = sb->s_magic;
	buf->f_bsize = sb->s_blocksize;
	buf->f_blocks = le32toh(sbi->s_es->s_blocks_count) - overhead;
	buf->f_bfree = le32toh(sbi->s_es->s_free_blocks_count);
	buf->f_bavail = buf->f_bfree - le32toh(sbi->s_es->s_r_blocks_count);
	if (buf->f_bfree < le32toh(sbi->s_es->s_r_blocks_count))
		buf->f_bavail = 0;
	buf->f_files = le32toh(sbi->s_es->s_inodes_count);
	buf->f_ffree = le32toh(sbi->s_es->s_free_inodes_count);
	buf->f_namelen = EXT2_NAME_LEN;
}

/*
 * Ext2 super operations.
 */
struct super_operations_t ext2_sops = {
	.read_inode		= ext2_read_inode,
	.write_inode		= ext2_write_inode,
	.put_inode		= ext2_put_inode,
	.statfs			= ext2_statfs,
};

/*
 * Ext2 file system.
 */
static struct file_system_t ext2_fs = {
	.name			= "ext2",
	.requires_dev		= 1,
	.read_super		= ext2_read_super,
};

/*
 * Init Ext2 file system.
 */
int init_ext2_fs()
{
	return register_filesystem(&ext2_fs);
}
