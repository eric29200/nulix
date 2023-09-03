#ifndef _RANDOM_H_
#define _RANDOM_H_

#include <fs/fs.h>

extern struct inode_operations random_iops;

int init_random();

#endif
