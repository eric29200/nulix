#include <fs/fs.h>
#include <fs/minix_fs.h>
#include <mm/mm.h>
#include <stderr.h>
#include <stdio.h>

/*
 * Read super block.
 */
static int minix_read_super(struct super_block *sb, void *data, int silent)
{
	struct minix3_super_block *msb3;
	struct minix_sb_info *sbi;
	int i, ret = -EINVAL;
	uint32_t block;

	/* unused data */
	UNUSED(data);

	/* allocate minix super block */
	sb->s_fs_info = sbi = (struct minix_sb_info *) kmalloc(sizeof(struct minix_sb_info));
	if (!sbi)
		return -ENOMEM;

	/* set default block size */
	set_blocksize(sb->s_dev, BLOCK_SIZE);
	sb->s_blocksize = BLOCK_SIZE;
	sb->s_blocksize_bits = BLOCK_SIZE_BITS;

	/* read super block */
	sbi->s_sbh = bread(sb->s_dev, 1, sb->s_blocksize);
	if (!sbi->s_sbh)
		goto err_bad_sb;

	/* check super block */
	msb3 = (struct minix3_super_block *) sbi->s_sbh->b_data;
	if (msb3->s_magic != MINIX3_MAGIC)
		goto err_bad_magic;

	/* set super block */
	sbi->s_ninodes = msb3->s_ninodes;
	sbi->s_nzones = msb3->s_zones;
	sbi->s_imap_blocks = msb3->s_imap_blocks;
	sbi->s_zmap_blocks = msb3->s_zmap_blocks;
	sbi->s_firstdatazone = msb3->s_firstdatazone;
	sbi->s_log_zone_size = msb3->s_log_zone_size;
	sbi->s_state = MINIX_VALID_FS;
	sbi->s_version = MINIX_V3;
	sbi->s_name_len = 60;
	sbi->s_dirsize = 64;
	sbi->s_max_size = msb3->s_max_size;
	sbi->s_imap = NULL;
	sbi->s_zmap = NULL;
	sb->s_magic = msb3->s_magic;
	sb->s_blocksize = msb3->s_blocksize;
	sb->s_blocksize_bits = blksize_bits(msb3->s_blocksize);
	sb->s_op = &minix_sops;
	sb->s_root = NULL;

	/* set real block size */
	set_blocksize(sb->s_dev, sb->s_blocksize);

	/* allocate inodes bitmap */
	sbi->s_imap = (struct buffer_head **) kmalloc(sizeof(struct buffer_head *) * sbi->s_imap_blocks);
	if (!sbi->s_imap) {
		ret = -ENOMEM;
		goto err_no_map;
	}

	/* reset inodes bitmap */
	for (i = 0; i < sbi->s_imap_blocks; i++)
		sbi->s_imap[i] = NULL;

	/* read inodes bitmap */
	for (i = 0, block = 2; i < sbi->s_imap_blocks; i++, block++) {
		sbi->s_imap[i] = bread(sb->s_dev, block, sb->s_blocksize);
		if (!sbi->s_imap[i]) {
			ret = -EIO;
			goto err_map;
		}
	}

	/* allocate zones bitmap */
	sbi->s_zmap = (struct buffer_head **) kmalloc(sizeof(struct buffer_head *) * sbi->s_zmap_blocks);
	if (!sbi->s_zmap) {
		ret = -ENOMEM;
		goto err_no_map;
	}

	/* reset zones bitmap */
	for (i = 0; i < sbi->s_zmap_blocks; i++)
		sbi->s_zmap[i] = NULL;

	/* read zones bitmap */
	for (i = 0; i < sbi->s_zmap_blocks; i++, block++) {
		sbi->s_zmap[i] = bread(sb->s_dev, block, sb->s_blocksize);
		if (!sbi->s_zmap[i]) {
			ret = -EIO;
			goto err_map;
		}
	}

	/* get root inode */
	sb->s_root = d_alloc_root(iget(sb, MINIX_ROOT_INODE));
	if (!sb->s_root) {
		ret = -EINVAL;
		goto err_root_inode;
	}

	return 0;
err_root_inode:
	if (!silent)
		printf("[Minix-fs] Can't read root inode\n");
	goto err_release_map;
err_map:
	if (!silent)
		printf("[Minix-fs] Can't read imap and zmap\n");
	goto err_release_map;
err_no_map:
	if (!silent)
		printf("[Minix-fs] Can't allocate imap and zmap\n");
err_release_map:
	if (sbi->s_imap) {
		for (i = 0; i < sbi->s_imap_blocks; i++)
			brelse(sbi->s_imap[i]);

		kfree(sbi->s_imap);
	}

	if (sbi->s_zmap) {
		for (i = 0; i < sbi->s_zmap_blocks; i++)
			brelse(sbi->s_zmap[i]);

		kfree(sbi->s_zmap);
	}

	goto err_release_sb;
err_bad_magic:
	if (!silent)
		printf("[Minix-fs] Bad magic number\n");
err_release_sb:
	brelse(sbi->s_sbh);
	goto err;
err_bad_sb:
	if (!silent)
		printf("[Minix-fs] Can't read super block\n");
err:
	kfree(sbi);
	return ret;
}

/*
 * Get statistics on file system.
 */
static void minix_statfs(struct super_block *sb, struct statfs64 *buf)
{
	struct minix_sb_info *sbi = minix_sb(sb);

	memset(buf, 0, sizeof(struct statfs64));
	buf->f_type = sb->s_magic;
	buf->f_bsize = sb->s_blocksize;
	buf->f_blocks = (sbi->s_nzones - sbi->s_firstdatazone) << sbi->s_log_zone_size;
	buf->f_bfree = minix_count_free_blocks(sb);
	buf->f_bavail = buf->f_bfree;
	buf->f_files = sbi->s_ninodes;
	buf->f_ffree = minix_count_free_inodes(sb);
	buf->f_namelen = sbi->s_name_len;
}

/*
 * Release a super block.
 */
static void minix_put_super(struct super_block *sb)
{
	struct minix_sb_info *sbi = minix_sb(sb);
	int i;

	/* release and free imap */
	for (i = 0; i < sbi->s_imap_blocks; i++)
		brelse(sbi->s_imap[i]);
	kfree(sbi->s_imap);

	/* release and free zmap */
	for (i = 0; i < sbi->s_zmap_blocks; i++)
		brelse(sbi->s_zmap[i]);
	kfree(sbi->s_imap);

	/* release super block buffer */
	brelse(sbi->s_sbh);

	/* free super block */
	kfree(sbi);

	sb->s_dev = 0;
}

/*
 * Minix super operations.
 */
struct super_operations minix_sops = {
	.put_super		= minix_put_super,
	.read_inode		= minix_read_inode,
	.write_inode		= minix_write_inode,
	.put_inode		= minix_put_inode,
	.statfs			= minix_statfs,
};

/*
 * Minix file system.
 */
static struct file_system minix_fs = {
	.name			= "minix",
	.requires_dev		= 1,
	.read_super		= minix_read_super,
};
/*
 * Init minix file system.
 */
int init_minix_fs()
{
	return register_filesystem(&minix_fs);
}
