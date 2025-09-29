#include <fs/proc_fs.h>
#include <stderr.h>
#include <stdio.h>

/*
 * Release a super block.
 */
static void proc_put_super(struct super_block *sb)
{
	UNUSED(sb);
}

/*
 * Superblock operations.
 */
static struct super_operations proc_sops = {
	.put_super		= proc_put_super,
	.read_inode		= proc_read_inode,
	.write_inode		= proc_write_inode,
	.put_inode		= proc_put_inode,
	.statfs			= proc_statfs,
};

/*
 * Read super block.
 */
static struct super_block *proc_read_super(struct super_block *sb, void *data, int silent)
{
	/* unused data */
	UNUSED(data);

	/* set super block */
	sb->s_magic = PROC_SUPER_MAGIC;
	sb->s_op = &proc_sops;

	/* get root inode */
	sb->s_root = d_alloc_root(iget(sb, PROC_ROOT_INO));
	if (!sb->s_root)
		goto err_root_inode;

	return sb;
err_root_inode:
	if (!silent)
		printf("[Proc-fs] Can't get root inode\n");
	sb->s_dev = 0;
	return NULL;
}

/*
 * Get statistics on file system.
 */
void proc_statfs(struct super_block *sb, struct statfs64 *buf)
{
	memset(buf, 0, sizeof(struct statfs64));
	buf->f_type = sb->s_magic;
	buf->f_bsize = sb->s_blocksize;
}

/*
 * Proc file system.
 */
static struct file_system_type proc_fs = {
	.name		= "proc",
	.flags		= 0,
	.read_super	= proc_read_super,
};

/*
 * Init proc file system.
 */
int init_proc_fs()
{
	return register_filesystem(&proc_fs);
}
