#include <mm/highmem.h>
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
	uint32_t page_nr;
	int i;

	for (i = 0; i < LAST_PKMAP; i++) {
		/* still used */
		if (pkmap_count[i] != 1)
			continue;

		/* free entry */
		pkmap_count[i] = 0;

		/* get page */
		page_nr = MAP_NR(pte_page(pkmap_page_table[i]));
		if (!page_nr || page_nr >= nr_pages)
			continue;

		/* clear entry */
		pte_clear(&pkmap_page_table[i]);
		page_array[page_nr].virtual = 0;
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
	pkmap_page_table[last_pkmap_nr] = mk_pte(page->page_nr, PAGE_KERNEL);
	page->virtual = vaddr;
	pkmap_count[last_pkmap_nr] = 1;

	/* flush TLB */
	flush_tlb_page(current_task->mm->pgd, vaddr);

	return vaddr;
}

/*
 * Map a high page in kernel address space.
 */
static void *kmap_high(struct page *page)
{
	uint32_t vaddr;

	/* map page in kernel space */
	vaddr = (uint32_t) page->virtual;
	if (!vaddr) {
		vaddr = map_new_virtual(page);
		if (!vaddr)
			goto out;
	}

	/* update reference count */
	pkmap_count[PKMAP_NR(vaddr)]++;
out:
	return (void *) vaddr;
}

/*
 * Unmap a high page in kernel address space.
 */
static void kunmap_high(struct page *page)
{
	uint32_t vaddr;
	uint32_t nr;

	/* page not mapped */
	vaddr = (uint32_t) page->virtual;
	if (!vaddr)
		return;

	/* decrement reference count */
	nr = PKMAP_NR(vaddr);
	pkmap_count[nr]--;
}

/*
 * Map a page in kernel adress space.
 */
void *kmap(struct page *page)
{
	if (page < highmem_start_page)
		return (void *) PAGE_ADDRESS(page);

	return kmap_high(page);
}

/*
 * Unmap a page in kernel adress space.
 */
void kunmap(struct page *page)
{
	if (page < highmem_start_page)
		return;

	kunmap_high(page);
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