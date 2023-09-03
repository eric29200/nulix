#ifndef _MOUSE_H_
#define _MOUSE_H_

#include <stddef.h>

int init_mouse();

extern struct inode_operations mouse_iops;

#endif
