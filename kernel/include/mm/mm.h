#ifndef _MM_H_
#define _MM_H_

#include <lib/list.h>
#include <stddef.h>

#define PAGE_SIZE			0x1000

#define KHEAP_START			0x400000				/* kernel memory : from 0 to 16 MB */
#define KHEAP_SIZE			0xF00000
#define KMEM_SIZE			(KHEAP_START + KHEAP_SIZE)
#define KPAGE_START			0x40000000				/* kernel pages : from 1 GB to 2 GB */
#define KPAGE_END			0x80000000
#define UMAP_START			0x80000000				/* user memory map : from 2 GB to 4 GB */
#define UMAP_END			0xF0000000
#define USTACK_START			0xF8000000				/* user stack */

/*
 * Virtual memory area structure.
 */
struct vm_area_t {
	uint32_t			vm_start;
	uint32_t			vm_end;
	uint16_t			vm_flags;
	off_t				vm_offset;
	struct inode_t *		vm_inode;
	struct vm_operations_t *	vm_ops;
	struct list_head_t		list;
};

/*
 * Virtual memory area operations.
 */
struct vm_operations_t {
	int (*nopage)(struct vm_area_t *, uint32_t address);
};

void init_mem(uint32_t start, uint32_t end);
void *kmalloc(uint32_t size);
void *kmalloc_align(uint32_t size);
void *kmalloc_align_phys(uint32_t size, uint32_t *phys);
void kfree(void *p);
void *get_free_page();
void free_page(void *address);
int generic_file_mmap(struct inode_t *inode, struct vm_area_t *vma);

#endif
