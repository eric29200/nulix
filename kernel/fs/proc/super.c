#include <fs/proc_fs.h>
#include <stderr.h>
#include <stdio.h>

/*
 * Superblock operations.
 */
static struct super_operations_t proc_sops = {
	.read_inode		= proc_read_inode,
	.write_inode		= proc_write_inode,
	.put_inode		= proc_put_inode,
};

/*
 * Read super block.
 */
static int proc_read_super(struct super_block_t *sb, void *data, int flags)
{
	/* unused data/flags */
	UNUSED(data);
	UNUSED(flags);

	/* set super block */
	sb->s_magic = PROC_SUPER_MAGIC;
	sb->s_op = &proc_sops;

	/* get root inode */
	sb->s_root_inode = iget(sb, PROC_ROOT_INO);
	if (!sb->s_root_inode) {
		printf("[Proc-fs] Can't get root inode\n");
		return -EACCES;
	}

	return 0;
}

/*
 * Proc file system.
 */
static struct file_system_t proc_fs = {
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
