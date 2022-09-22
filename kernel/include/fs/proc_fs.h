#ifndef _PROC_FS_H_
#define _PROC_FS_H_

#include <fs/fs.h>
#include <string.h>

#define PROC_SUPER_MAGIC	0x9FA0

#define PROC_ROOT_INO		1
#define PROC_UPTIME_INO		2
#define PROC_FILESYSTEMS_INO	3
#define PROC_MOUNTS_INO		4
#define PROC_SELF_INO		5
#define PROC_KSTAT_INO		6
#define PROC_MEMINFO_INO	7
#define PROC_LOADAVG_INO	8
#define PROC_BASE_INO		1000

/*
 * Procfs dir entry.
 */
struct proc_dir_entry_t {
	ino_t	ino;
	size_t	name_len;
	char *	name;
};

/* super operations */
int init_proc_fs();

/* inode  operations */
int proc_read_inode(struct inode_t *inode);
int proc_write_inode(struct inode_t *inode);
int proc_put_inode(struct inode_t *inode);
void proc_statfs(struct super_block_t *sb, struct statfs64_t *buf);

/* inode operations */
extern struct inode_operations_t proc_root_iops;
extern struct inode_operations_t proc_base_iops;
extern struct inode_operations_t proc_uptime_iops;
extern struct inode_operations_t proc_loadavg_iops;
extern struct inode_operations_t proc_filesystems_iops;
extern struct inode_operations_t proc_mounts_iops;
extern struct inode_operations_t proc_self_iops;
extern struct inode_operations_t proc_kstat_iops;
extern struct inode_operations_t proc_meminfo_iops;

/*
 * Test if a name matches a directory entry.
 */
static inline int proc_match(const char *name, size_t len, struct proc_dir_entry_t *de)
{
	return len == de->name_len && strncmp(name, de->name, len) == 0;
}

#endif
