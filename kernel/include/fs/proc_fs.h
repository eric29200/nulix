#ifndef _PROC_FS_H_
#define _PROC_FS_H_

#include <fs/fs.h>
#include <string.h>

#define PROC_SUPER_MAGIC	0x9FA0

#define FIRST_PROCESS_ENTRY	256

#define PROC_DYNAMIC_FIRST	4096
#define PROC_NDYNAMIC		4096

/* root directory */
#define PROC_ROOT_INO		1
#define PROC_SELF_INO		2

/* /<pid> directories */
#define PROC_PID_INO		2
#define PROC_PID_STAT_INO	3
#define PROC_PID_STATUS_INO	4
#define PROC_PID_STATM_INO	5
#define PROC_PID_CMDLINE_INO	6
#define PROC_PID_ENVIRON_INO	7
#define PROC_PID_FD_INO		8

/* /<pid>/fd directories */
#define PROC_PID_FD_DIR		0x8000

/* read proc prototype */
typedef	int (read_proc_t)(char *, char **, off_t off, size_t, int *);

/*
 * Procfs dir entry.
 */
struct proc_dir_entry {
	ino_t				low_ino;
	size_t				name_len;
	char *				name;
	mode_t				mode;
	size_t				nlink;
	uid_t				uid;
	gid_t				gid;
	size_t				size;
	struct inode_operations *	ops;
	read_proc_t *			read_proc;
	struct proc_dir_entry *		next;
	struct proc_dir_entry *		parent;
	struct proc_dir_entry *		subdir;
};

/* procfs init operations */
int init_proc_fs();
void proc_root_init();
void proc_misc_init();
void proc_base_init();
void proc_net_init();

/* procfs generic operations */
int proc_register(struct proc_dir_entry *dir, struct proc_dir_entry *de);
struct proc_dir_entry *create_proc_entry(const char *name, mode_t mode, struct proc_dir_entry *parent);
struct proc_dir_entry *create_proc_read_entry(const char *name, mode_t mode, struct proc_dir_entry *dir, read_proc_t *read_proc);
struct proc_dir_entry *proc_mkdir_mode(const char *name, mode_t mode, struct proc_dir_entry *dir);
struct proc_dir_entry *proc_mkdir(const char *name, struct proc_dir_entry *dir);
int proc_readdir(struct file *filp, void *dirent, filldir_t filldir);
struct dentry *proc_lookup(struct inode *dir, struct dentry *dentry);

/* procfs pid operations */
int proc_pid_readdir(struct file *filp, void *dirent, filldir_t filldir);
struct dentry *proc_pid_lookup(struct inode *dir, struct dentry *dentry);

/* procfs array operations */
int proc_stat_read(struct task *task, char *page);
int proc_status_read(struct task *task, char *page);
int proc_statm_read(struct task *task, char *page);
int proc_cmdline_read(struct task *task, char *page);
int proc_environ_read(struct task *task, char *page);

/* inode operations */
struct inode *proc_get_inode(struct super_block *sb, ino_t ino, struct proc_dir_entry *de);
int proc_read_inode(struct inode *inode);
int proc_write_inode(struct inode *inode);
int proc_put_inode(struct inode *inode);
void proc_statfs(struct super_block *sb, struct statfs64 *buf);

/* directories entries */
extern struct proc_dir_entry proc_root;
extern struct proc_dir_entry proc_pid;
extern struct proc_dir_entry *proc_net;

/* inode operations */
extern struct inode_operations proc_root_iops;
extern struct inode_operations proc_link_iops;
extern struct inode_operations proc_dir_iops;
extern struct inode_operations proc_file_iops;

/*
 * Test if a name matches a directory entry.
 */
static inline int proc_match(const char *name, size_t len, struct proc_dir_entry *de)
{
	return len == de->name_len && strncmp(name, de->name, len) == 0;
}

#endif
