#ifndef _HIGHMEM_H_
#define _HIGHMEM_H_

#include <mm/paging.h>
#include <string.h>

#define PKMAP_BASE		0xFF800000
#define LAST_PKMAP		1024
#define LAST_PKMAP_MASK		(LAST_PKMAP - 1)

#define PKMAP_NR(vaddr)		(((vaddr) - PKMAP_BASE) >> PAGE_SHIFT)
#define PKMAP_ADDR(nr)		(PKMAP_BASE + ((nr) << PAGE_SHIFT))

extern pte_t *pkmap_page_table;
extern struct page *highmem_start_page;

void *kmap(struct page *page);
void kunmap(struct page *page);
void clear_user_highpage(struct page *page);
void clear_user_highpage_partial(struct page *page, off_t offset);
void copy_user_highpage(struct page *dst, struct page *src);

#endif