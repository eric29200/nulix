#ifndef _PTY_H_
#define _PTY_H_

extern struct inode_operations_t ptmx_iops;
extern struct inode_operations_t pty_iops;

int init_pty();

#endif
