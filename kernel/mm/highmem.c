#include <mm/highmem.h>
#include <drivers/block/blk_dev.h>
#include <stdio.h>
#include <string.h>
#include <proc/sched.h>

static uint32_t last_pkmap_nr = LAST_PKMAP;
static int pkmap_count[LAST_PKMAP];
struct page *highmem_start_page;
pte_t *pkmap_page_table;

/*
 * Clear unused virtual mapping.
 */
static void flush_all_zero_pkmaps()
{
	struct page *page;
	int i;

	for (i = 0; i < LAST_PKMAP; i++) {
		/* still used */
		if (pkmap_count[i] != 1)
			continue;

		/* free entry */
		pkmap_count[i] = 0;

		/* no entry */
		if (pte_none(pkmap_page_table[i]))
			continue;

		/* get page */
		page = pte_page(pkmap_page_table[i]);
		if (!VALID_PAGE(page))
			continue;

		/* clear entry */
		pte_clear(&pkmap_page_table[i]);
		page->virtual = NULL;
	}

	/* flush tlb */
	flush_tlb(current_task->mm->pgd);
}

/*
 * Create a new virtual mapping.
 */
static uint32_t map_new_virtual(struct page *page)
{
	int count = LAST_PKMAP;
	uint32_t vaddr;

	/* find first free pkmap entry */
	for (;;) {
		last_pkmap_nr = (last_pkmap_nr + 1) & LAST_PKMAP_MASK;

		/* flush unused pkmaps */
		if (!last_pkmap_nr) {
			flush_all_zero_pkmaps();
			count = LAST_PKMAP;
		}

		/* free entry found */
		if (!pkmap_count[last_pkmap_nr])
			break;

		/* try next entry */
		if (--count)
			continue;

		return 0;
	}

	/* set pte */
	vaddr = PKMAP_ADDR(last_pkmap_nr);
	pkmap_page_table[last_pkmap_nr] = mk_pte(page, PAGE_KERNEL);
	page->virtual = (void *) vaddr;
	pkmap_count[last_pkmap_nr] = 1;

	/* flush TLB */
	flush_tlb_page(current_task->mm->pgd, vaddr);

	return vaddr;
}

/*
 * Map a page in kernel adress space.
 */
void *kmap(struct page *page)
{
	uint32_t vaddr;

	/* page already mapped */
	vaddr = (uint32_t) page->virtual;
	if (vaddr)
		goto mapped;

	/* map page in kernel space */
	vaddr = map_new_virtual(page);
	if (vaddr)
		goto mapped;

	return NULL;
mapped:
	/* update reference count */
	pkmap_count[PKMAP_NR(vaddr)]++;
	return (void *) vaddr;
}

/*
 * Unmap a page in kernel adress space.
 */
void kunmap(struct page *page)
{
	uint32_t vaddr;

	/* page not mapped */
	vaddr = (uint32_t) page->virtual;
	if (!vaddr)
		return;

	/* reset virtual address */
	page->virtual = NULL;

	/* decrement reference count */
	pkmap_count[PKMAP_NR(vaddr)]--;
}

/*
 * Clear a user high page.
 */
void clear_user_highpage(struct page *page)
{
	char *vpage = kmap(page);
	memset(vpage, 0, PAGE_SIZE);
	kunmap(page);
}

/*
 * Clear a user high page.
 */
void clear_user_highpage_partial(struct page *page, off_t offset)
{
	char *vpage = kmap(page);
	memset(vpage + offset, 0, PAGE_SIZE - offset);
	kunmap(page);
}

/*
 * Copy a user high page.
 */
void copy_user_highpage(struct page *dst, struct page *src)
{
	char *vdst, *vsrc;

	/* map pages in kernel adress space */
	vdst = kmap(dst);
	vsrc = kmap(src);

	/* copy page */
	memcpy(vdst, vsrc, PAGE_SIZE);

	/* unmap pages */
	kunmap(src);
	kunmap(dst);
}
