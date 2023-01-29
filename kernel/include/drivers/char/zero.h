#ifndef _ZERO_H_
#define _ZERO_H_

#include <fs/fs.h>

extern struct inode_operations_t zero_iops;

int init_zero();

#endif
