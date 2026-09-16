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
	flush_tlb(current->mm->pgd);
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
	flush_tlb_page(current->mm->pgd, vaddr);

	return vaddr;
}

/*
 * Map a page in kernel adress space.
 */
void *kmap(struct page *page)
{
	uint32_t vaddr;

	/* not a highmem page */
	if (page < highmem_start_page)
		return page_address(page);

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

/*
 * Copy from high memory to a buffer.
 */
static void copy_from_high_bh(struct buffer_head *to, struct buffer_head *from)
{
	struct page *p_from = from->b_page;
	memcpy(to->b_data, kmap(p_from) + bh_offset(from), to->b_size);
	kunmap(p_from);
}

/*
 * Copy a buffer to high memory.
 */
static void copy_to_high_bh(struct buffer_head *to, struct buffer_head *from)
{
	struct page *p_to = to->b_page;
	memcpy(kmap(p_to) + bh_offset(to), from->b_data, to->b_size);
	kunmap(p_to);
}

/*
 * End a bounce buffer read/write.
 */
static void bounce_end_io(struct buffer_head *bh, int uptodate)
{
	struct buffer_head *bh_orig = (struct buffer_head *) bh->b_private;

	/* end original buffer */
	bh_orig->b_end_io(bh_orig, uptodate);
	bh_orig->b_private = NULL;

	/* free bounce buffer */
	__free_page(bh->b_page);
	kfree(bh);
}

/*
 * End a bounce buffer write.
 */
static void bounce_end_io_write(struct buffer_head *bh, int uptodate)
{
	bounce_end_io(bh, uptodate);
}

/*
 * End a bounce buffer read.
 */
static void bounce_end_io_read(struct buffer_head *bh, int uptodate)
{
	struct buffer_head *bh_orig = (struct buffer_head *) bh->b_private;

	/* copy data to original buffer */
	if (uptodate)
		copy_to_high_bh(bh_orig, bh);

	bounce_end_io(bh, uptodate);
}

/*
 * Create a bounce buffer.
 */
struct buffer_head *create_bounce(int rw, struct buffer_head *bh_orig)
{
	struct buffer_head *bh;
	struct page *page;

	/* allocate bounce buffer */
	bh = (struct buffer_head *) kmalloc(sizeof(struct buffer_head));
	if (!bh)
		return NULL;

	/* get a free page */
	page = __get_free_page(GFP_KERNEL);
	if (!page)
		goto err;

	/* set new buffer */
	memset(bh, 0, sizeof(struct buffer_head));
	bh->b_data = page_address(page);
	bh->b_page = page;
	bh->b_block = bh_orig->b_block;
	bh->b_size = bh_orig->b_size;
	bh->b_dev = bh_orig->b_dev;
	bh->b_count = bh_orig->b_count;
	bh->b_state = bh_orig->b_state;
	bh->b_rsector = bh_orig->b_rsector;
	bh->b_private = (void *) bh_orig;

	if (rw == WRITE) {
		bh->b_end_io = bounce_end_io_write;
		copy_from_high_bh(bh, bh_orig);
	} else {
		bh->b_end_io = bounce_end_io_read;
	}

	return bh;
err:
	kfree(bh);
	return NULL;
}