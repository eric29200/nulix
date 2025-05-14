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
#define PROC_PID_INO		9
#define PROC_PID_STAT_INO	10
#define PROC_PID_STATUS_INO	11
#define PROC_PID_CMDLINE_INO	12
#define PROC_PID_ENVIRON_INO	13
#define PROC_PID_FD_INO		14
#define PROC_NET_INO		15
#define PROC_NET_DEV_INO	16

/*
 * Procfs dir entry.
 */
struct proc_dir_entry {
	ino_t	ino;
	size_t	name_len;
	char *	name;
};

/* super operations */
int init_proc_fs();

/* inode operations */
int proc_read_inode(struct inode *inode);
int proc_write_inode(struct inode *inode);
int proc_put_inode(struct inode *inode);
void proc_statfs(struct super_block *sb, struct statfs64 *buf);

/* inode operations */
extern struct inode_operations proc_root_iops;
extern struct inode_operations proc_array_iops;
extern struct inode_operations proc_base_dir_iops;
extern struct inode_operations proc_base_iops;
extern struct inode_operations proc_self_iops;
extern struct inode_operations proc_fd_iops;
extern struct inode_operations proc_fd_link_iops;
extern struct inode_operations proc_net_iops;
extern struct inode_operations proc_net_dev_iops;

/*
 * Test if a name matches a directory entry.
 */
static inline int proc_match(const char *name, size_t len, struct proc_dir_entry *de)
{
	return len == de->name_len && strncmp(name, de->name, len) == 0;
}

#endif
