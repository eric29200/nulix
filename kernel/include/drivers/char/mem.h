#ifndef _MEM_H_
#define _MEM_H_

#include <fs/fs.h>

int random_read(struct file *filp, char *buf, int n);

extern struct inode_operations memory_iops;

#endif