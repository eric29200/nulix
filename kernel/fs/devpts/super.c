#include <fs/devpts_fs.h>
#include <stdio.h>
#include <string.h>
#include <stderr.h>

/*
 * Get statistics on file system.
 */
static void devpts_statfs(struct super_block *sb, struct statfs64 *buf)
{
	memset(buf, 0, sizeof(struct statfs64));
	buf->f_type = sb->s_magic;
	buf->f_bsize = sb->s_blocksize;
}

/*
 * Release a super block.
 */
static void devpts_put_super(struct super_block *sb)
{
	sb->s_dev = 0;
}

/*
 * Superblock operations.
 */
static struct super_operations devpts_sops = {
	.put_super		= devpts_put_super,
	.read_inode		= devpts_read_inode,
	.write_inode		= devpts_write_inode,
	.put_inode		= devpts_put_inode,
	.statfs			= devpts_statfs,
};

/*
 * Read super block.
 */
static int devpts_read_super(struct super_block *sb, void *data, int silent)
{
	struct devpts_entry *root_entry;
	int ret = -EINVAL;

	/* unused data */
	UNUSED(data);

	/* set super block */
	sb->s_magic = DEVPTS_SUPER_MAGIC;
	sb->s_op = &devpts_sops;

	/* get or create root entry */
	root_entry = devpts_get_root_entry();
	if (!root_entry)
		goto err_no_root_entry;

	/* get root inode */
	sb->s_root_inode = devpts_iget(sb, root_entry);
	if (!sb->s_root_inode)
		goto err_no_root_inode;

	return 0;
err_no_root_inode:
	if (!silent)
		printf("[Devpts] Can't get root inode\n");
	goto err;
err_no_root_entry:
	if (!silent)
		printf("[Devpts] Can't get root entry\n");
err:
	return ret;
}

/*
 * Devpts file system.
 */
static struct file_system devpts_fs = {
	.name		= "devpts",
	.requires_dev	= 0,
	.read_super	= devpts_read_super,
};

/*
 * Init devpts file system.
 */
int init_devpts_fs()
{
	return register_filesystem(&devpts_fs);
}
