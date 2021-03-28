#ifndef _MM_MEM_H_
#define _MM_MEM_H_

#include <lib/stddef.h>

#define PAGE_SIZE       0x1000    /* 4 kB */

void init_mem(uint32_t start, uint32_t end);

#endif
