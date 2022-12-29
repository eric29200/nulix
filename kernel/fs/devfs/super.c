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
 * Superblock operations.
 */
static struct super_operations_t devfs_sops = {
	.read_inode		= devfs_read_inode,
	.write_inode		= devfs_write_inode,
	.put_inode		= devfs_put_inode,
	.statfs			= devfs_statfs,
};

/*
 * Create root inode.
 */
static struct inode_t *devfs_mkroot(struct super_block_t *sb)
{
	struct inode_t *inode;
	int ret;

	/* create root inode */
	inode = devfs_new_inode(sb, S_IFDIR | 0755, 0);
	if (!inode)
		return NULL;

	/* create "." entry in root directory */
	ret = devfs_add_entry(inode, ".", 1, inode);
	if (ret)
		goto err;

	/* create ".." entry in root directory */
	ret = devfs_add_entry(inode, "..", 2, inode);
	if (ret)
		goto err;

	return inode;
err:
	inode->i_nlinks = 0;
	iput(inode);
	return NULL;
}

/*
 * Read super block.
 */
static int devfs_read_super(struct super_block_t *sb, void *data, int silent)
{
	/* unused data */
	UNUSED(data);

	/* set super block */
	sb->s_magic = DEVFS_SUPER_MAGIC;
	sb->s_op = &devfs_sops;

	/* get root inode */
	sb->s_root_inode = devfs_mkroot(sb);
	if (!sb->s_root_inode) {
		if (!silent)
			printf("[Dev-fs] Can't create root inode\n");

		return -EACCES;
	}

	return 0;
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
