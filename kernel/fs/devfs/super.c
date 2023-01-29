#include <fs/dev_fs.h>
#include <stdio.h>
#include <stderr.h>
#include <fcntl.h>

/*
 * Get statistics on file system.
 */
static void devfs_statfs(struct super_block_t *sb, struct statfs64_t *buf)
{
	memset(buf, 0, sizeof(struct statfs64_t));
	buf->f_type = sb->s_magic;
	buf->f_bsize = sb->s_blocksize;
}

/*
 * Release a super block.
 */
static void devfs_put_super(struct super_block_t *sb)
{
	sb->s_dev = 0;
}

/*
 * Superblock operations.
 */
static struct super_operations_t devfs_sops = {
	.put_super		= devfs_put_super,
	.read_inode		= devfs_read_inode,
	.write_inode		= devfs_write_inode,
	.put_inode		= devfs_put_inode,
	.statfs			= devfs_statfs,
};

/*
 * Read super block.
 */
static int devfs_read_super(struct super_block_t *sb, void *data, int silent)
{
	struct devfs_entry_t *root_entry;
	int ret = -EINVAL;

	/* unused data */
	UNUSED(data);

	/* set super block */
	sb->s_magic = DEVFS_SUPER_MAGIC;
	sb->s_op = &devfs_sops;

	/* get or create root entry */
	root_entry = devfs_get_root_entry();
	if (!root_entry)
		goto err_no_root_entry;

	/* get root inode */
	sb->s_root_inode = devfs_iget(sb, root_entry);
	if (!sb->s_root_inode)
		goto err_no_root_inode;

	return 0;
err_no_root_inode:
	if (!silent)
		printf("[Dev-fs] Can't get root inode\n");
	goto err;
err_no_root_entry:
	if (!silent)
		printf("[Dev-fs] Can't get root entry\n");
err:
	return ret;
}

/*
 * Dev file system.
 */
static struct file_system_t dev_fs = {
	.name		= "devfs",
	.requires_dev	= 0,
	.read_super	= devfs_read_super,
};

/*
 * Init dev file system.
 */
int init_dev_fs()
{
	return register_filesystem(&dev_fs);
}
