#ifndef _MMAP_H_
#define _MMAP_H_

#include <fs/fs.h>
#include <lib/list.h>
#include <stddef.h>

#define VM_READ			0x01
#define VM_WRITE		0x02
#define VM_EXEC			0x04
#define VM_SHARED		0x08

#define MAP_SHARED       	1
#define MAP_PRIVATE      	2
#define MAP_TYPE         	0xF
#define MAP_FIXED        	0x10
#define MAP_ANONYMOUS    	0x20

/*
 * Virtual memory area structure.
 */
struct vm_area_t {
	uint32_t		vm_start;
	uint32_t		vm_end;
	uint16_t		vm_flags;
	struct list_head_t	list;
};

void *do_mmap(uint32_t addr, size_t length, int flags, struct file_t *filp, off_t offset);
int do_munmap(uint32_t addr, size_t length);
struct vm_area_t *find_vma_intersection(struct task_t *task, uint32_t start, uint32_t end);

#endif
