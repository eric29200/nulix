#ifndef _RANDOM_H_
#define _RANDOM_H_

#include <fs/fs.h>

extern struct inode_operations_t random_iops;

int init_random_dev();

#endif
