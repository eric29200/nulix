#ifndef _NULL_H_
#define _NULL_H_

#include <fs/fs.h>

extern struct inode_operations_t null_iops;

int init_null();

#endif
