#ifndef _MEM_H_
#define _MEM_H_

#include <fs/fs.h>

int random_read(struct file *filp, char *buf, size_t n);
int init_mem_devices();

#endif