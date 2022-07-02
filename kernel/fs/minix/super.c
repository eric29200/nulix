#include <fs/fs.h>
#include <fs/minix_fs.h>
#include <mm/mm.h>
#include <stderr.h>
#include <stdio.h>

/*
 * Minix super operations.
 */
struct super_operations_t minix_sops = {
	.read_inode		= minix_read_inode,
	.write_inode		= minix_write_inode,
	.put_inode		= minix_put_inode,
};

/*
 * Read super block.
 */
static int minix_read_super(struct super_block_t *sb, void *data, int flags)
{
	struct minix_super_block_t *msb;
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

	/* read minix super block */
	msb = (struct minix_super_block_t *) sbi->s_sbh->b_data;

	/* check magic number */
	if (msb->s_magic != MINIX_SUPER_MAGIC) {
		ret = -EINVAL;
		goto err_magic;
	}

	/* set root super block */
	sbi->s_ninodes = msb->s_ninodes;
	sbi->s_nzones = msb->s_zones;
	sbi->s_imap_blocks = msb->s_imap_blocks;
	sbi->s_zmap_blocks = msb->s_zmap_blocks;
	sbi->s_firstdatazone = msb->s_firstdatazone;
	sbi->s_log_zone_size = msb->s_log_zone_size;
	sbi->s_imap = NULL;
	sbi->s_zmap = NULL;
	sbi->s_max_size = msb->s_max_size;
	sb->s_magic = msb->s_magic;
	sb->s_root_inode = NULL;
	sb->s_op = &minix_sops;

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
		for (i = 0; i < msb->s_imap_blocks; i++)
			brelse(sbi->s_imap[i]);

		kfree(sbi->s_imap);
	}

	if (sbi->s_zmap) {
		for (i = 0; i < msb->s_zmap_blocks; i++)
			brelse(sbi->s_zmap[i]);

		kfree(sbi->s_zmap);
	}

	goto err_release_sb;
err_magic:
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
 * Minix file system.
 */
static struct file_system_t minix_fs = {
	.name		= "minix",
	.requires_dev	= 1,
	.read_super	= minix_read_super,
};

/*
 * Init minix file system.
 */
int init_minix_fs()
{
	return register_filesystem(&minix_fs);
}
