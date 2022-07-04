#include <fs/fs.h>
#include <fs/minix_fs.h>
#include <mm/mm.h>
#include <stderr.h>
#include <stdio.h>

/*
 * Read super block.
 */
static int minix_read_super(struct super_block_t *sb, void *data, int flags)
{
	struct minix1_super_block_t *msb1;
	struct minix3_super_block_t *msb3;
	struct minix_sb_info_t *sbi;
	uint32_t block;
	int i, ret;

	/* unused data/flags */
	UNUSED(data);
	UNUSED(flags);

	/* allocate minix super block */
	sb->s_fs_info = sbi = (struct minix_sb_info_t *) kmalloc(sizeof(struct minix_sb_info_t));
	if (!sbi)
		return -ENOMEM;

	/* set default block size */
	sb->s_blocksize = MINIX_BLOCK_SIZE;
	sb->s_blocksize_bits = MINIX_BLOCK_SIZE_BITS;

	/* read super block */
	sbi->s_sbh = bread(sb, 1);
	if (!sbi->s_sbh)
		goto err_bad_sb;

	/* set Minix file system */
	msb1 = (struct minix1_super_block_t *) sbi->s_sbh->b_data;
	sbi->s_ninodes = msb1->s_ninodes;
	sbi->s_nzones = msb1->s_zones;
	sbi->s_imap_blocks = msb1->s_imap_blocks;
	sbi->s_zmap_blocks = msb1->s_zmap_blocks;
	sbi->s_firstdatazone = msb1->s_firstdatazone;
	sbi->s_log_zone_size = msb1->s_log_zone_size;
	sbi->s_state = msb1->s_state;
	sbi->s_imap = NULL;
	sbi->s_zmap = NULL;
	sbi->s_max_size = msb1->s_max_size;
	sb->s_magic = msb1->s_magic;
	sb->s_root_inode = NULL;
	sb->s_op = &minix_sops;

	/* set Minix file system specific version */
	if (sb->s_magic == MINIX1_MAGIC1) {
		sbi->s_version = MINIX_V1;
		sbi->s_name_len = 14;
		sbi->s_dirsize = 16;
	} else if (sb->s_magic == MINIX1_MAGIC2) {
		sbi->s_version = MINIX_V1;
		sbi->s_name_len = 30;
		sbi->s_dirsize = 32;
	} else if (sb->s_magic == MINIX2_MAGIC1) {
		sbi->s_version = MINIX_V2;
		sbi->s_name_len = 14;
		sbi->s_dirsize = 16;
	} else if (sb->s_magic == MINIX2_MAGIC2) {
		sbi->s_version = MINIX_V2;
		sbi->s_name_len = 30;
		sbi->s_dirsize = 32;
	} else if (*((uint16_t *) (sbi->s_sbh->b_data + 24)) == MINIX3_MAGIC) {
		msb3 = (struct minix3_super_block_t *) sbi->s_sbh->b_data;
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
		sb->s_blocksize = msb3->s_blocksize;
		sb->s_blocksize_bits = blksize_bits(msb3->s_blocksize);
	} else {
		goto err_bad_magic;
	}

	/* allocate inodes bitmap */
	sbi->s_imap = (struct buffer_head_t **) kmalloc(sizeof(struct buffer_head_t *) * sbi->s_imap_blocks);
	if (!sbi->s_imap) {
		ret = -ENOMEM;
		goto err_no_map;
	}

	/* reset inodes bitmap */
	for (i = 0; i < sbi->s_imap_blocks; i++)
		sbi->s_imap[i] = NULL;

	/* read inodes bitmap */
	for (i = 0, block = 2; i < sbi->s_imap_blocks; i++, block++) {
		sbi->s_imap[i] = bread(sb, block);
		if (!sbi->s_imap[i]) {
			ret = -EIO;
			goto err_map;
		}
	}

	/* allocate zones bitmap */
	sbi->s_zmap = (struct buffer_head_t **) kmalloc(sizeof(struct buffer_head_t *) * sbi->s_zmap_blocks);
	if (!sbi->s_zmap) {
		ret = -ENOMEM;
		goto err_no_map;
	}

	/* reset zones bitmap */
	for (i = 0; i < sbi->s_zmap_blocks; i++)
		sbi->s_zmap[i] = NULL;

	/* read zones bitmap */
	for (i = 0; i < sbi->s_zmap_blocks; i++, block++) {
		sbi->s_zmap[i] = bread(sb, block);
		if (!sbi->s_zmap[i]) {
			ret = -EIO;
			goto err_map;
		}
	}

	/* get root inode */
	sb->s_root_inode = iget(sb, MINIX_ROOT_INODE);
	if (!sb->s_root_inode) {
		ret = -EINVAL;
		goto err_root_inode;
	}

	return 0;
err_root_inode:
	printf("[Minix-fs] Can't read root inode\n");
	goto err_release_map;
err_map:
	printf("[Minix-fs] Can't read imap and zmap\n");
	goto err_release_map;
err_no_map:
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
	printf("[Minix-fs] Bad magic number\n");
err_release_sb:
	brelse(sbi->s_sbh);
	goto err;
err_bad_sb:
	printf("[Minix-fs] Can't read super block\n");
err:
	kfree(sbi);
	return ret;
}

/*
 * Get statistics on file system.
 */
static void minix_statfs(struct super_block_t *sb, struct statfs64_t *buf)
{
	struct minix_sb_info_t *sbi = minix_sb(sb);

	memset(buf, 0, sizeof(struct statfs64_t));
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
 * Minix super operations.
 */
struct super_operations_t minix_sops = {
	.read_inode		= minix_read_inode,
	.write_inode		= minix_write_inode,
	.put_inode		= minix_put_inode,
	.statfs			= minix_statfs,
};

/*
 * Minix file system.
 */
static struct file_system_t minix_fs = {
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
