#include <fs/proc_fs.h>
#include <stderr.h>
#include <stdio.h>

/*
 * Release a super block.
 */
static void proc_put_super(struct super_block *sb)
{
	sb->s_dev = 0;
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
static int proc_read_super(struct super_block *sb, void *data, int silent)
{
	/* unused data */
	UNUSED(data);

	/* set super block */
	sb->s_magic = PROC_SUPER_MAGIC;
	sb->s_op = &proc_sops;

	/* get root inode */
	sb->s_root_inode = iget(sb, PROC_ROOT_INO);
	if (!sb->s_root_inode) {
		if (!silent)
			printf("[Proc-fs] Can't get root inode\n");

		return -EACCES;
	}

	return 0;
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
static struct file_system proc_fs = {
	.name		= "proc",
	.requires_dev	= 0,
	.read_super	= proc_read_super,
};

/*
 * Init proc file system.
 */
int init_proc_fs()
{
	return register_filesystem(&proc_fs);
}
