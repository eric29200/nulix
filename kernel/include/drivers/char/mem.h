#ifndef _MEM_H_
#define _MEM_H_

#include <fs/fs.h>

int random_read(struct file *filp, char *buf, int n);
int init_mem_devices();

#endif