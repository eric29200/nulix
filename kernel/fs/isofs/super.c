#include <fs/fs.h>
#include <fs/iso_fs.h>
#include <mm/mm.h>
#include <stderr.h>
#include <stdio.h>

/*
 * Read super block.
 */
static int isofs_read_super(struct super_block_t *sb, void *data, int silent)
{
	struct iso_directory_record_t *root_dir;
	struct iso_primary_descriptor_t *pri;
	struct iso_volume_descriptor_t *vdp;
	struct isofs_sb_info_t *sbi;
	struct buffer_head_t *sbh;
	int block, ret = -EINVAL;

	/* unused data */
	UNUSED(data);

	/* allocate ISOFS super block */
	sb->s_fs_info = sbi = (struct isofs_sb_info_t *) kmalloc(sizeof(struct isofs_sb_info_t));
	if (!sbi)
		return -ENOMEM;

	/* set default block size */
	set_blocksize(sb->s_dev, ISOFS_BLOCK_SIZE);
	sb->s_blocksize = ISOFS_BLOCK_SIZE;
	sb->s_blocksize_bits = ISOFS_BLOCK_SIZE_BITS;

	/* try to find volume descriptor */
	for (block = 16, pri = NULL; block < 100; block++) {
		/* read next block */
		sbh = bread(sb->s_dev, block, sb->s_blocksize);
		if (!sbh)
			goto err_primary_vol;

		/* get volume descriptor */
		vdp = (struct iso_volume_descriptor_t *) sbh->b_data;

		/* check volume id */
		if (strncmp(vdp->id, "CD001", sizeof(vdp->id)) == 0) {
			/* check volume type */
			if (isofs_num711(vdp->type) != ISOFS_VD_PRIMARY)
				goto err_primary_vol;

			/* get primary descriptor */
			pri = (struct iso_primary_descriptor_t *) vdp;
			break;
		}

		/* release super block buffer */
		brelse(sbh);
	}

	/* no primary volume */
	if (!pri)
		goto err_primary_vol;

	/* check if file system is not multi volumes */
	if (isofs_num723(pri->volume_set_size) != 1)
		goto err_multivol;

	/* get root directory record */
	root_dir = (struct iso_directory_record_t *) pri->root_directory_record;

	/* set super block */
	sbi->s_nzones = isofs_num733(pri->volume_space_size);
	sbi->s_log_zone_size = blksize_bits(isofs_num723(pri->logical_block_size));
	sbi->s_max_size = isofs_num733(pri->volume_space_size);
	sbi->s_ninodes = 0;
	sbi->s_firstdatazone = (isofs_num733(root_dir->extent) + isofs_num711(root_dir->ext_attr_length)) << sbi->s_log_zone_size;
	sb->s_magic = ISOFS_MAGIC;
	sb->s_op = &isofs_sops;
	sb->s_root_inode = NULL;

	/* release super block buffer */
	brelse(sbh);

	/* get root inode */
	sb->s_root_inode = iget(sb, sbi->s_firstdatazone);
	if (!sb->s_root_inode)
		goto err_root_inode;

	return 0;
err_root_inode:
	if (!silent)
		printf("[ISO-fs] Can't get root inode\n");
	goto err;
err_multivol:
	if (!silent)
		printf("[ISO-fs] Multi volume disks not supported\n");
	goto err_release_sb;
err_primary_vol:
	if (!silent)
		printf("[ISO-fs] Can't find primary volume descriptor\n");
	goto err_release_sb;
err_release_sb:
	brelse(sbh);
err:
	kfree(sbi);
	return ret;
}

/*
 * Get statistics on file system.
 */
static void isofs_statfs(struct super_block_t *sb, struct statfs64_t *buf)
{
	struct isofs_sb_info_t *sbi = isofs_sb(sb);

	memset(buf, 0, sizeof(struct statfs64_t));
	buf->f_type = sb->s_magic;
	buf->f_bsize = sb->s_blocksize;
	buf->f_blocks = sbi->s_nzones << (sbi->s_log_zone_size - sb->s_blocksize_bits);
	buf->f_bfree = 0;
	buf->f_bavail = 0;
	buf->f_files = sbi->s_ninodes;
	buf->f_ffree = 0;
	buf->f_namelen = ISOFS_MAX_NAME_LEN;
}

/*
 * Release a super block.
 */
static void isofs_put_super(struct super_block_t *sb)
{
	struct isofs_sb_info_t *sbi = isofs_sb(sb);

	/* free super block */
	kfree(sbi);

	sb->s_dev = 0;
}

/*
 * Isofs super operations.
 */
struct super_operations_t isofs_sops = {
	.put_super		= isofs_put_super,
	.read_inode		= isofs_read_inode,
	.statfs			= isofs_statfs,
};

/*
 * Isofs file system.
 */
static struct file_system_t iso_fs = {
	.name			= "isofs",
	.requires_dev		= 1,
	.read_super		= isofs_read_super,
};
/*
 * Init iso file system.
 */
int init_iso_fs()
{
	return register_filesystem(&iso_fs);
}
