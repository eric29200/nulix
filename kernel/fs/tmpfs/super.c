#include <fs/tmp_fs.h>
#include <stdio.h>
#include <stderr.h>
#include <fcntl.h>

/*
 * Get statistics on file system.
 */
static void tmpfs_statfs(struct super_block_t *sb, struct statfs64_t *buf)
{
	memset(buf, 0, sizeof(struct statfs64_t));
	buf->f_type = sb->s_magic;
	buf->f_bsize = sb->s_blocksize;
}

/*
 * Superblock operations.
 */
static struct super_operations_t tmpfs_sops = {
	.read_inode		= tmpfs_read_inode,
	.write_inode		= tmpfs_write_inode,
	.put_inode		= tmpfs_put_inode,
	.statfs			= tmpfs_statfs,
};

/*
 * Create root inode.
 */
static struct inode_t *tmpfs_mkroot(struct super_block_t *sb)
{
	struct inode_t *inode;
	int ret;
	
	/* create root inode */
	inode = tmpfs_new_inode(sb, S_IFDIR | 0755, 0);
	if (!inode)
		return NULL;

	/* create "." entry in root directory */
	ret = tmpfs_add_entry(inode, ".", 1, inode);
	if (ret)
		goto err;

	/* create ".." entry in root directory */
	ret = tmpfs_add_entry(inode, "..", 2, inode);
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
static int tmpfs_read_super(struct super_block_t *sb, void *data, int silent)
{
	/* unused data */
	UNUSED(data);

	/* set super block */
	sb->s_magic = TMPFS_SUPER_MAGIC;
	sb->s_op = &tmpfs_sops;

	/* get root inode */
	sb->s_root_inode = tmpfs_mkroot(sb);
	if (!sb->s_root_inode) {
		if (!silent)
			printf("[Tmp-fs] Can't create root inode\n");

		return -EACCES;
	}

	return 0;
}

/*
 * Tmp file system.
 */
static struct file_system_t tmp_fs = {
	.name		= "tmp",
	.requires_dev	= 0,
	.read_super	= tmpfs_read_super,
};

/*
 * Init tmp file system.
 */
int init_tmp_fs()
{
	return register_filesystem(&tmp_fs);
}
