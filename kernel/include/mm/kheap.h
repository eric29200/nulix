#ifndef _MM_KHEAP_H_
#define _MM_KHEAP_H_

#include <mm/paging.h>
#include <stddef.h>

#define HEAP_MAGIC		0xAEA0

/*
 * Heap block header.
 */
struct heap_block {
	uint16_t		magic;
	uint32_t		size;
	uint8_t			free;
	struct heap_block *	prev;
	struct heap_block *	next;
} __attribute__((packed));

/*
 * Heap structure.
 */
struct kheap {
	struct heap_block *	first_block;
	uint32_t		start_address;
	uint32_t		end_address;
	size_t			size;
};

/* kernel heap (defined in kheap.c) */
extern struct kheap *kheap;

int kheap_init(uint32_t start_address, size_t size);
void *kheap_alloc(size_t size, uint8_t page_aligned);
void kheap_free(void *p);
void kheap_dump();

#endif
