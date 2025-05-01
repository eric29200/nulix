#ifndef _MM_H_
#define _MM_H_

#include <lib/list.h>
#include <mm/paging.h>
#include <mm/kheap.h>
#include <stddef.h>

#define KHEAP_START			0x400000				/* kernel code + heap : from 0 to 16 MB */
#define KHEAP_SIZE			0xF00000
#define UMAP_START			0x40000000				/* user memory map : from 1 GB to 3 GB */
#define UMAP_END			(PAGE_OFFSET)
#define KPAGE_START			(PAGE_OFFSET)				/* kernel pages : from 3 GB to 4 GB */
#define KPAGE_END			0xF0000000
#define USTACK_START			0xF8000000				/* user stack */
#define USTACK_LIMIT			(8 * 1024 * 1024)			/* user stack limit = 8 MB */

/*
 * Virtual memory area structure.
 */
struct vm_area {
	uint32_t			vm_start;
	uint32_t			vm_end;
	uint16_t			vm_flags;
	uint32_t			vm_page_prot;
	off_t				vm_offset;
	struct inode *			vm_inode;
	struct vm_operations *		vm_ops;
	struct list_head		list;
	struct list_head		list_share;
};

/*
 * Virtual memory area operations.
 */
struct vm_operations {
	void (*open)(struct vm_area *);
	void (*close)(struct vm_area *);
	struct page *(*nopage)(struct vm_area *, uint32_t);
};

void init_mem(uint32_t start, uint32_t end);

#endif
