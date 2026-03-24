#include <proc/sched.h>
#include <drivers/block/blk_dev.h>
#include <mm/swap.h>
#include <stdio.h>
#include <kernel_stat.h>
#include <stderr.h>
#include <fcntl.h>

/* global variables */
static struct swap_info swap_info[MAX_SWAPFILES];
static size_t nr_swapfiles = 0;
static struct inode swapper_inode;
static int least_priority = 0;
static LIST_HEAD(swap_list);

/*
 * Find a swap page in cache.
 */
static struct page *lookup_swap_cache(uint32_t entry)
{
	struct page *page;

	for (;;) {
		page = find_page(&swapper_inode, entry);
		if (!page)
			return NULL;
		if (page->inode != &swapper_inode || !PageSwapCache(page))
			goto err;
		if (!PageLocked(page))
			return page;
		__free_page(page);
		wait_on_page(page);
	}

err:
	printf("lookup_swap_cache: found a non-swapper swap page\n");
	__free_page(page);
	return 0;
}

/*
 * Cache a swap page.
 */
static int add_to_swap_cache(struct page *page, uint32_t entry)
{
	if (PageSwapCache(page) || page->inode) {
		printf("swap_cache: replacing non-empty entry %08lx on page %08lx\n", page->offset, page_address(page));
		return 0;
	}

	SetPageSwapCache(page);
	page->flags = page->flags & ~(1 << PG_uptodate);
	add_to_page_cache(page, &swapper_inode, entry);

	return 1;
}

/*
 * Remove a page from swap cache.
 */
static void remove_from_swap_cache(struct page *page)
{
	if (!page->inode) {
		printf("remove_from_swap_cache: removing swap cache page with zero inode hash on page %08lx\n", page_address(page));
		return;
	}

	if (page->inode != &swapper_inode)
		printf("remove_from_swap_cache: removing swap cache page with wrong inode hash on page %08lx\n", page_address(page));

	ClearPageSwapCache(page);
	remove_from_page_cache(page);
}

/*
 * Find a free swap entry.
 */
static int scan_swap_map(struct swap_info *si)
{
	uint32_t offset;

	for (offset = si->lowest_bit; offset <= si->highest_bit; offset++) {
		if (si->swap_map[offset])
			continue;

		si->lowest_bit = offset;
		si->swap_map[offset] = 1;
		if (offset == si->highest_bit)
			si->highest_bit--;

		return offset;
	}

	return 0;
}

/*
 * Get a swap page.
 */
static uint32_t get_swap_page()
{
	struct list_head *pos;
	struct swap_info *si;
	uint32_t offset;

	list_for_each(pos, &swap_list) {
		si = list_entry(pos, struct swap_info, list);
		if ((si->flags & SWP_WRITEOK) != SWP_WRITEOK)
			continue;

		offset = scan_swap_map(si);
		if (offset)
			return SWP_ENTRY(si->type, offset);
	}

	return 0;
}


/*
 * Get reference count of a swap entry.
 */
static int swap_count(uint32_t entry)
{
	uint32_t offset, type;
	struct swap_info *si;

	if (!entry)
		goto err_entry;

	/* get swap file */
	type = SWP_TYPE(entry);
	if (type >= nr_swapfiles)
		goto err_file;
	si = &swap_info[type];

	/* check offset */
	offset = SWP_OFFSET(entry);
	if (offset >= si->max)
		goto err_offset;
	if (!si->swap_map[offset])
		goto err_unused;

	return si->swap_map[offset];
err_entry:
	printf("swap_count: null entry\n");
	goto err;
err_file:
	printf("swap_count: entry %08lx, nonexistent swap file\n", entry);
	goto err;
err_offset:
	printf("swap_count: entry %08lx, offset exceeds max\n", entry);
	goto err;
err_unused:
	printf("swap_count at %8p: entry %08lx, unused page\n", __builtin_return_address(0), entry);
err:
	return 0;
}

/*
 * Free a swap entry.
 */
void swap_free(uint32_t entry)
{
	uint32_t offset, type;
	struct swap_info *si;

	if (!entry)
		return;

	/* get swap file */
	type = SWP_TYPE(entry);
	if (type >= nr_swapfiles)
		goto err_nofile;
	si = &swap_info[type];

	/* check swap file */
	if (!(si->flags & SWP_USED))
		goto err_device;

	/* check offset */
	offset = SWP_OFFSET(entry);
	if (offset >= si->max)
		goto err_offset;
	if (!si->swap_map[offset])
		goto err_free;

	/* update reference count */
	if (si->swap_map[offset] < SWAP_MAP_MAX) {
		if (!--si->swap_map[offset]) {
			if (offset < si->lowest_bit)
				si->lowest_bit = offset;
			if (offset > si->highest_bit)
				si->highest_bit = offset;
		}
	}

	return;
err_nofile:
	printf("swap_free: trying to free nonexistent swap-page\n");
	return;
err_device:
	printf("swap_free: trying to free swap from unused swap-device\n");
	return;
err_offset:
	printf("swap_free: offset exceeds max\n");
	return;
err_free:
	printf("swap_free: swap-space map bad (entry %08lx)\n", entry);
}

/*
 * Verify that a swap entry is valid and increment its swap map count.
 */
static int swap_duplicate(uint32_t entry)
{
	static int overflow = 0;
	uint32_t offset, type;
	struct swap_info *si;

	if (!entry)
		return 0;

	/* get swap file */
	type = SWP_TYPE(entry);
	if (type >= nr_swapfiles)
		goto err_file;
	si = &swap_info[type];

	/* check offset */
	offset = SWP_OFFSET(entry);
	if (offset >= si->max)
		goto err_offset;
	if (!si->swap_map[offset])
		goto err_unused;

	/* entry is valid, so increment the map count */
	if (si->swap_map[offset] < SWAP_MAP_MAX) {
		si->swap_map[offset]++;
	} else {
		if (overflow++ < 5)
			printf("swap_duplicate: entry %08lx map count=%d\n", entry, si->swap_map[offset]);
		si->swap_map[offset] = SWAP_MAP_MAX;
	}

	return 1;
err_file:
	printf("swap_duplicate: entry %08lx, nonexistent swap file\n", entry);
	goto err;
err_offset:
	printf("swap_duplicate: entry %08lx, offset exceeds max\n", entry);
	goto err;
err_unused:
	printf("swap_duplicate at %8p: entry %08lx, unused page\n", __builtin_return_address(0), entry);
err:
	return 0;
}

/*
 * Read/write a swap page.
 */
static void rw_swap_page_base(int rw, uint32_t entry, struct page *page)
{
	uint32_t type, offset, block, blocks[PAGE_SIZE / 512];
	size_t block_size, nr_blocks;
	struct inode *swap_i;
	struct swap_info *p;
	dev_t dev;
	int i, j;

	/* get swap file */
	type = SWP_TYPE(entry);
	if (type >= nr_swapfiles) {
		printf("rw_swap_page_base: bad swap file/device\n");
		return;
	}

	/* check offset */
	p = &swap_info[type];
	offset = SWP_OFFSET(entry);
	if (offset >= p->max) {
		printf("rw_swap_page_base: bad offset\n");
		return;
	}

	/* check offset */
	if (p->swap_map && !p->swap_map[offset]) {
		printf("rw_swap_page_base: Trying to %s unallocated swap (%08lx)\n",  rw == READ ? "read" : "write", entry);
		return;
	}

	/* check swap file */
	if (!(p->flags & SWP_USED)) {
		printf("rw_swap_page_base: trying to swap to unused swap file/device\n");
		return;
	}

	/* swap page must be locked */
	if (!PageLocked(page)) {
		printf("rw_swap_page_base: swap page is not locked\n");
		return;
	}

	/* check page */
	if (PageSwapCache(page) && page->offset != entry) {
		printf("rw_swap_page_base: swap entry mismatch\n");
		return;
	}

	if (rw == READ) {
		clear_bit(&page->flags, PG_uptodate);
		kstat.pswpin++;
	} else {
		kstat.pswpout++;
	}

	/* update page reference count */
	page->count++;

	/* get blocks to read/write */
	if (p->swap_device) {
		blocks[0] = offset;
		nr_blocks = 1;
		dev = p->swap_device;
		block_size = PAGE_SIZE;
	} else if (p->swap_file) {
		swap_i = p->swap_file->d_inode;

		block = offset << (PAGE_SHIFT - swap_i->i_sb->s_blocksize_bits);
		block_size = swap_i->i_sb->s_blocksize;

		for (i = 0, j = 0; j < PAGE_SIZE ; i++, j += block_size) {
			blocks[i] = swap_i->i_op->bmap(swap_i, block++);
			if (!blocks[i]) {
				printf("rw_swap_page_base: bad swap file\n");
				return;
			}
		}

		nr_blocks = i;
		dev = swap_i->i_dev;
	} else {
		printf("rw_swap_page_base: no swap file or device\n");
		page->count--;
		return;
	}

	 /* read/write page */
 	brw_page(rw, page, dev, blocks, nr_blocks, block_size);
	wait_on_page(page);
	__free_page(page);
}

/*
 * Read/write a swap page.
 */
static void rw_swap_page(int rw, uint32_t entry, char *buffer)
{
	struct page *page = &page_array[MAP_NR(buffer)];

	/* check page */
	if (page->inode && page->inode != &swapper_inode)
		panic("rw_swap_page: tried to swap a non swapper page\n");
	if (!PageSwapCache(page)) {
		printf("rw_swap_page: swap page is not in swap cache\n");
		return;
	}
	if (page->offset != entry) {
		printf("rw_swap_page: swap entry mismatch\n");
		return;
	}

	/* read/write swap page */
	rw_swap_page_base(rw, entry, page);
}

/*
 * Read/write a swap page (do not store page in cache).
 */
static void rw_swap_page_nocache(int rw, uint32_t entry, char *buffer)
{
	struct page *page = &page_array[MAP_NR(buffer)];

	/* lock page */
	wait_on_page(page);
	set_bit(&page->flags, PG_lock);

	/* page already in swap cache */
	if (test_bit(&page->flags, PG_swap_cache) || page->inode) {
		printf("read_swap_page_nocache: page already in swap cache\n");
		return;
	}

	/* read/write page */
	set_bit(&page->flags, PG_swap_cache);
	page->inode = &swapper_inode;
	page->offset = entry;
	page->count++;
	rw_swap_page(rw, entry, buffer);
	page->count--;
	page->inode = NULL;
	clear_bit(&page->flags, PG_swap_cache);
}

/*
 * Read a page from disk and store it in cache.
 */
static struct page *read_swap_cache(uint32_t entry)
{
	struct page *found_page = NULL, *new_page;

	/* make sure the swap entry is still in use */
	if (!swap_duplicate(entry))
		goto out;

	/* look for the page in the swap cache */
	found_page = lookup_swap_cache(entry);
	if (found_page)
		goto out_free_swap;

	/* get a new page */
	new_page = __get_free_page(GFP_KERNEL);
	if (!new_page)
		goto out_free_swap;

	/* check the swap cache again, in case we stalled above */
	found_page = lookup_swap_cache(entry);
	if (found_page)
		goto out_free_page;

	/* add it to the swap cache */
	if (!add_to_swap_cache(new_page, entry))
		goto out_free_page;

	/* read page from disk */
	set_bit(&new_page->flags, PG_lock);
	rw_swap_page(READ, entry, page_address(new_page));

	return new_page;
out_free_page:
	__free_page(new_page);
out_free_swap:
	swap_free(entry);
out:
	return found_page;
}

/*
 * Check if swap page is shared.
 */
static int is_page_shared(struct page *page)
{
	int count;

	if (PageReserved(page))
		return 1;

	count = page->count;
	if (PageSwapCache(page))
		count += swap_count(page->offset) - 2;

	return count > 1;
}

/*
 * Delete a page from swap cache.
 */
static void delete_from_swap_cache(struct page *page)
{
	uint32_t entry = page->offset;

	remove_from_swap_cache(page);
	swap_free(entry);
}

/*
 * Free a swap page and remove it from cache.
 */
void free_page_and_swap_cache(struct page *page)
{
	/* if we are the only user, then free up the swap cache */
	if (PageSwapCache(page) && !is_page_shared(page))
		delete_from_swap_cache(page);

	__free_page(page);
}

/*
 * Swap in a page.
 */
int swap_in(struct vm_area *vma, pte_t *pte, uint32_t entry, int write_access)
{
	struct page *page;

	/* try to find swap page in cache */
	page = lookup_swap_cache(entry);

	/* read page on disk if needed */
	if (!page)
		page = read_swap_cache(entry);

	/* check entry */
	if (*pte != entry) {
		if (page)
			free_page_and_swap_cache(page);
		return 1;
	}

	if (!page)
		return -1;

	/* free swap entry */
	vma->vm_mm->rss++;
	swap_free(entry);

	/* read only access : update page table entry */
	if (!write_access || is_page_shared(page)) {
		*pte = mk_pte(page, vma->vm_page_prot);
		return 1;
	}

	/* page is unshared and we're going to dirty it : remove it from cache */
	delete_from_swap_cache(page);
	*pte = pte_mkwrite(pte_mkdirty(mk_pte(page, vma->vm_page_prot)));

  	return 1;
}

/*
 * Try to swap out a page.
 */
static int try_to_swap_out(struct vm_area *vma, uint32_t address, pte_t *pte)
{
	struct page *page;
	uint32_t entry;

	/* page not present */
	if (!pte_present(*pte))
		return 0;

	/* non valid page */
	page = pte_page(*pte);
	if (!VALID_PAGE(page))
		return 0;

	/* skip reserved pages */
	if (PageReserved(page))
		return 0;

	/* page already in swap cache */
	if (PageSwapCache(page)) {
		entry = page->offset;
		swap_duplicate(entry);
		*pte = entry;
		goto drop_pte;
	}

	/* inode page */
	if (page->inode) {
		/* clean page : drop pte */
		if (!pte_dirty(*pte)) {
			pte_clear(pte);
			goto drop_pte;
		}

		/* swapout not implemented */
		if (!vma->vm_ops || !vma->vm_ops->swapout)
			return 0;

		/* swap out page */
		vma->vm_ops->swapout(vma, page);
		pte_clear(pte);
		goto drop_pte;
	}

	/* get a free swap page */
	entry = get_swap_page();
	if (!entry)
		return 0;

	/* update page table */
	vma->vm_mm->rss--;
	*pte = entry;
	flush_tlb_page(vma->vm_mm->pgd, address);

	/* store page in swap cache */
	swap_duplicate(entry);
	add_to_swap_cache(page, entry);
	set_bit(&page->flags, PG_lock);

	/* write page on disk */
	rw_swap_page(WRITE, entry, page_address(page));
	__free_page(page);

	return 1;
drop_pte:
	vma->vm_mm->rss--;
	flush_tlb_page(vma->vm_mm->pgd, address);
	__free_page(page);
	return 1;
}

/*
 * Try to swap out a page table.
 */
static int swap_out_pmd(struct vm_area *vma, pmd_t *pmd, uint32_t address, uint32_t end)
{
	uint32_t pmd_end;
	pte_t *pte;
	int ret;

	if (pmd_none(*pmd))
		return 0;

	pte = pte_offset(pmd, address);

	pmd_end = (address + PMD_SIZE) & PMD_MASK;
	if (end > pmd_end)
		end = pmd_end;

	do {
		vma->vm_mm->swap_address = address + PAGE_SIZE;

		ret -= try_to_swap_out(vma, address, pte);
		if (ret)
			return ret;

		address += PAGE_SIZE;
		pte++;
	} while (address < end);

	return 0;
}

/*
 * Try to swap out a page directory.
 */
static int swap_out_pgd(struct vm_area *vma, pgd_t *pgd, uint32_t address, uint32_t end)
{
	uint32_t pgd_end;
	pmd_t *pmd;
	int ret;

	pmd = pmd_offset(pgd);

	pgd_end = (address + PGDIR_SIZE) & PGDIR_MASK;
	if (end > pgd_end)
		end = pgd_end;

	do {
		ret = swap_out_pmd(vma, pmd, address, end);
		if (ret)
			return ret;

		address = (address + PMD_SIZE) & PMD_MASK;
		pmd++;
	} while (address < end);

	return 0;
}

/*
 * Try to swap out a memory region.
 */
static int swap_out_vma(struct vm_area *vma, uint32_t address)
{
	uint32_t end;
	pgd_t *pgd;
	int ret;

	/* don't try to swap out locked regions */
	if (vma->vm_flags & VM_LOCKED)
		return 0;

	pgd = pgd_offset(vma->vm_mm->pgd, address);

	for (end = vma->vm_end; address < end;) {
		ret = swap_out_pgd(vma, pgd, address, end);
		if (ret)
			return ret;

		address = (address + PGDIR_SIZE) & PGDIR_MASK;
		pgd++;
	}

	return 0;
}

/*
 * Try to swap out a process.
 */
static int swap_out_process(struct task *task)
{
	struct vm_area *vma;
	uint32_t address;
	int ret;

	/* start at swap address */
	address = task->mm->swap_address;

	/* find memory region to swap */
	vma = find_vma(task, address);
	if (!vma)
		goto out;

	if (address < vma->vm_start)
		address = vma->vm_start;

	for (;;) {
		/* try to swap out memory region */
		ret = swap_out_vma(vma, address);
		if (ret)
			return ret;

		/* try next memory region */
		vma = list_next_entry_or_null(vma, &task->mm->vm_list, list);
		if (!vma)
			break;

		address = vma->vm_start;
	}

out:
	task->mm->swap_address = 0;
	task->mm->swap_cnt = 0;
	return 0;
}

/*
 * Find best task to swap out.
 */
static struct task *__find_best_task_to_swap_out(int assign)
{
	struct task *task, *best = NULL;
	struct list_head *pos;
	uint32_t max_cnt = 0;

	list_for_each(pos, &tasks_list) {
		task = list_entry(pos, struct task, list);

		if (task->mm->rss <= 0)
			continue;

		/* assign swap counter ? */
		if (assign == 1)
			task->mm->swap_cnt = task->mm->rss;

		/* find task with max memory */
		if (task->mm->swap_cnt > max_cnt) {
			max_cnt = task->mm->swap_cnt;
			best = task;
		}
	}

	return best;
}

/*
 * Try to swap out some process.
 */
int swap_out(int priority)
{
	struct task *task;
	int count, ret;

	/* number of tasks to scan */
	count = nr_tasks / priority;
	if (count < 1)
		count = 1;

	for (; count >= 0; count--) {
		/* find best task to swap out */
		task = __find_best_task_to_swap_out(0);
		if (!task) {
			/* reassign swap counter */
			task = __find_best_task_to_swap_out(1);
			if (!task)
				return 0;
		}

		/* try to swap out process */
		ret = swap_out_process(task);
		if (ret)
			return ret;
	}

	return 0;
}

/*
 * Insert swap file/device in swap list.
 */
static void insert_swap_info(struct swap_info *si)
{
	struct swap_info *tmp;
	struct list_head *pos;

	/* find insert position in swap list */
	list_for_each(pos, &swap_list) {
		tmp = list_entry(pos, struct swap_info, list);
		if (si->priority >= tmp->priority)
			goto found;
	}

	list_add_tail(&si->list, &swap_list);
	return;
found:
	list_add_tail(&si->list, &tmp->list);
}

/*
 * Start swapping to file/device.
 */
int sys_swapon(const char *path, int swap_flags)
{
	size_t swap_file_size, nr_good_pages = 0, i;
	union swap_header *swap_header = NULL;
	struct dentry *swap_dentry;
	struct file filp = { 0 };
	struct swap_info *p;
	uint32_t type;
	int ret, page;
	dev_t dev;

	/* find a free swap file */
	for (type = 0, p = swap_info; type < nr_swapfiles ; type++, p++)
		if (!(p->flags & SWP_USED))
			break;

	/* no free swap file */
	if (type >= MAX_SWAPFILES)
		return -EPERM;

	/* update number of swap files */
	if (type >= nr_swapfiles)
		nr_swapfiles = type + 1;

	/* init swap file/device */
	p->type = type;
	p->flags = SWP_USED;
	p->swap_file = NULL;
	p->swap_device = 0;
	p->swap_map = NULL;
	p->lowest_bit = 0;
	p->highest_bit = 0;
	p->max = 1;
	if (swap_flags & SWAP_FLAG_PREFER)
		p->priority = (swap_flags & SWAP_FLAG_PRIO_MASK) >> SWAP_FLAG_PRIO_SHIFT;
	else
		p->priority = --least_priority;

	/* resolve path */
	swap_dentry = namei(AT_FDCWD, path, 1);
	ret = PTR_ERR(swap_dentry);
	if (IS_ERR(swap_dentry))
		goto err;
	p->swap_file = swap_dentry;

	/* open block device or regular file */
	ret = -EINVAL;
	if (S_ISBLK(swap_dentry->d_inode->i_mode)) {
		/* configure device */
		dev = swap_dentry->d_inode->i_rdev;
		p->swap_device = dev;
		set_blocksize(dev, PAGE_SIZE);

		/* open block device */
		filp.f_dentry = swap_dentry;
		filp.f_mode = 3;
		ret = blkdev_open(swap_dentry->d_inode, &filp);
		if (ret)
			goto err;

		/* set block size */
		set_blocksize(dev, PAGE_SIZE);
		ret = -ENODEV;
		if (!dev || (blk_size[major(dev)] && !blk_size[major(dev)][minor(dev)]))
			goto err;

		/* check if device is aleady used */
		ret = -EBUSY;
		for (i = 0 ; i < nr_swapfiles ; i++) {
			if (i == type)
				continue;
			if (dev == swap_info[i].swap_device)
				goto err;
		}

		/* get device size */
		swap_file_size = 0;
		if (blk_size[major(dev)])
			swap_file_size = blk_size[major(dev)][minor(dev)] >> (PAGE_SHIFT - 10);
	} else if (S_ISREG(swap_dentry->d_inode->i_mode)) {
		/* check if swap file is already used */
		ret = -EBUSY;
		for (i = 0 ; i < nr_swapfiles ; i++) {
			if (i == type || !swap_info[i].swap_file)
				continue;
			if (swap_dentry->d_inode == swap_info[i].swap_file->d_inode)
				goto err;
		}

		/* get file size */
		swap_file_size = swap_dentry->d_inode->i_size >> PAGE_SHIFT;
	} else {
		goto err;
	}

	/* allocate memory to read swap header */
	swap_header = (union swap_header *) get_free_page();
	if (!swap_header) {
		ret = -ENOMEM;
		goto err;
	}

	/* read swap header */
	rw_swap_page_nocache(READ, SWP_ENTRY(type, 0), (char *) swap_header);

	/* check magic number */
	ret = -EINVAL;
	if (memcmp("SWAPSPACE2", swap_header->magic.magic, 10) || swap_header->info.version != 1)
		goto err;

	/* read header */
	p->lowest_bit = 1;
	p->highest_bit = swap_header->info.last_page - 1;
	p->max = swap_header->info.last_page;

	/* check max offset */
	if (p->max >= SWP_OFFSET(SWP_ENTRY(0, ~0UL)))
		goto err;

	/* allocate swap map */
	p->swap_map = kmalloc(p->max * sizeof(uint16_t));
	if (!p->swap_map) {
		ret = -ENOMEM;
		goto err;
	}

	/* parse bad pages */
	ret = 0;
	memset(p->swap_map, 0, p->max * sizeof(short));
	for (i = 0; i<swap_header->info.nr_badpages; i++) {
		page = swap_header->info.badpages[i];
		if (page <= 0 || (uint32_t) page >= swap_header->info.last_page)
			ret = -EINVAL;
		else
			p->swap_map[page] = SWAP_MAP_BAD;
	}

	/* compute number of swap pages */
	nr_good_pages = swap_header->info.last_page - swap_header->info.nr_badpages - 1;
	if (ret)
		goto err;

	/* check swap file size */
	if (swap_file_size && p->max > swap_file_size) {
		printf("sys_swapon: swap area shorter than signature indicates\n");
		ret = -EINVAL;
		goto err;
	}

	/* check number of good pages */
	if (!nr_good_pages) {
		printf("sys_swapon: empty swap-file\n");
		ret = -EINVAL;
		goto err;
	}

	/* set swap file/device */
	p->swap_map[0] = SWAP_MAP_BAD;
	p->flags = SWP_WRITEOK;
	printf("Adding Swap: %dk swap-space (priority %d)\n", nr_good_pages << (PAGE_SHIFT - 10), p->priority);

	/* insert swap file/device in swap list */
	insert_swap_info(p);

	ret = 0;
	goto out;
err:
	if (filp.f_op && filp.f_op->release)
		filp.f_op->release(filp.f_dentry->d_inode, &filp);
	if (p->swap_map)
		kfree(p->swap_map);
	if (p->swap_file)
		dput(p->swap_file);
	p->type = 0;
	p->swap_device = 0;
	p->swap_file = NULL;
	p->swap_map = NULL;
	p->flags = 0;
	if (!(swap_flags & SWAP_FLAG_PREFER))
		++least_priority;
out:
	if (swap_header)
		free_page(swap_header);
	return ret;
}

/*
 * Try to unuse a swap page.
 */
static void unuse_pte(struct vm_area *vma, pte_t *pte, uint32_t entry, struct page *page)
{
	if (pte_none(*pte))
		return;

	if (pte_present(*pte)) {
		/* If this entry is swap-cached, then page must already
                   hold the right address for any copies in physical
                   memory */
		if (pte_page(*pte) != page)
			return;
		/* We will be removing the swap cache in a moment, so... */
		*pte = pte_mkdirty(*pte);
		return;
	}
	if (*pte != entry)
		return;

	*pte = pte_mkdirty(mk_pte(page, vma->vm_page_prot));
	swap_free(entry);
	page->count++;
}

/*
 * Try to unuse a swap page.
 */
static void unuse_pmd(struct vm_area *vma, pmd_t *pmd, uint32_t address, size_t size, uint32_t offset, uint32_t entry, struct page *page)
{
	uint32_t end;
	pte_t *pte;

	if (pmd_none(*pmd))
		return;

	pte = pte_offset(pmd, address);
	offset += address & PMD_MASK;
	address &= ~PMD_MASK;
	end = address + size;
	if (end > PMD_SIZE)
		end = PMD_SIZE;

	do {
		unuse_pte(vma, pte, entry, page);
		address += PAGE_SIZE;
		pte++;
	} while (address < end);
}

/*
 * Try to unuse a swap page.
 */
static void unuse_pgd(struct vm_area *vma, pgd_t *pgd, uint32_t address, size_t size, uint32_t entry, struct page *page)
{
	uint32_t offset, end;
	pmd_t *pmd;

	pmd = pmd_offset(pgd);
	offset = address & PGDIR_MASK;
	address &= ~PGDIR_MASK;
	end = address + size;
	if (end > PGDIR_SIZE)
		end = PGDIR_SIZE;

	do {
		unuse_pmd(vma, pmd, address, end - address, offset, entry, page);
		address = (address + PMD_SIZE) & PMD_MASK;
		pmd++;
	} while (address < end);
}

/*
 * Try to unuse a swap page.
 */
static void unuse_vma(struct vm_area *vma, pgd_t *pgd, uint32_t entry, struct page *page)
{
	uint32_t start = vma->vm_start, end = vma->vm_end;

	while (start < end) {
		unuse_pgd(vma, pgd, start, end - start, entry, page);
		start = (start + PGDIR_SIZE) & PGDIR_MASK;
		pgd++;
	}
}

/*
 * Try to unuse a swap page.
 */
static void unuse_process(struct mm_struct *mm, uint32_t entry, struct page *page)
{
	struct list_head *pos;
	struct vm_area *vma;
	pgd_t *pgd;

	if (!mm || mm == init_task->mm)
		return;

	list_for_each(pos, &mm->vm_list) {
		vma = list_entry(pos, struct vm_area, list);
		pgd = pgd_offset(mm->pgd, vma->vm_start);
		unuse_vma(vma, pgd, entry, page);
	}
}

/*
 * Try to unuse a swap file.
 */
static int try_to_unuse(struct swap_info *si)
{
	struct list_head *pos;
	struct page *page;
	struct task *task;
	uint32_t entry;
	size_t i;

	for (;;) {
		/* find a swap page in use */
		for (i = 1; i < si->max ; i++)
			if (si->swap_map[i] > 0 && si->swap_map[i] != SWAP_MAP_BAD)
				goto found_entry;

		/* no swap page in use */
		break;
found_entry:
		/* get swap entry */
		entry = SWP_ENTRY(si->type, i);

		/* read page from disk */
		page = read_swap_cache(entry);
		if (!page) {
			if (si->swap_map[i] == 0)
				continue;
  			return -ENOMEM;
		}

		/* try to unuse swap pages for each process */
		list_for_each(pos, &tasks_list) {
			task = list_entry(pos, struct task, list);
			unuse_process(task->mm, entry, page);
		}

		/* delete page from swap cache */
		if (PageSwapCache(page))
			delete_from_swap_cache(page);
		__free_page(page);

		/* check for and clear any overflowed swap map counts */
		if (si->swap_map[i]) {
			if (si->swap_map[i] != SWAP_MAP_MAX)
				printf("try_to_unuse: entry %08lx count=%d\n", entry, si->swap_map[i]);
			si->swap_map[i] = 0;
		}
	}

	return 0;
}

/*
 * Stop swapping to file/device.
 */
int sys_swapoff(const char *path)
{
	struct swap_info *si = NULL;
	struct file filp = { 0 };
	struct dentry *dentry;
	struct list_head *pos;
	int ret;

	/* resolve path */
	dentry = namei(AT_FDCWD, path, 1);
	ret = PTR_ERR(dentry);
	if (IS_ERR(dentry))
		return ret;

	/* find swap file */
	list_for_each(pos, &swap_list) {
		si = list_entry(pos, struct swap_info, list);
		if ((si->flags & SWP_WRITEOK) == SWP_WRITEOK) {
			if (si->swap_file == dentry)
				break;
			if (S_ISBLK(dentry->d_inode->i_mode) && si->swap_device == dentry->d_inode->i_rdev)
				break;
		}

		si = NULL;
	}

	/* can't find swap file */
	ret = -EINVAL;
	if (!si)
		goto out_dput;

	/* try to unused swap file */
	si->flags = SWP_USED;
	ret = try_to_unuse(si);
	if (ret) {
		si->flags = SWP_WRITEOK;
		goto out_dput;
	}

	/* release block device */
	if (si->swap_device) {
		filp.f_dentry = dentry;
		filp.f_mode = 3;

		/* open it again to get fops */
		if (!blkdev_open(dentry->d_inode, &filp) && filp.f_op && filp.f_op->release) {
			filp.f_op->release(dentry->d_inode,&filp);
			filp.f_op->release(dentry->d_inode,&filp);
		}
	}

	/* clear swap file */
	list_del(&si->list);
	dput(dentry);
	dentry = si->swap_file;
	si->swap_file = NULL;
	si->swap_device = 0;
	kfree(si->swap_map);
	si->swap_map = NULL;
	si->flags = 0;
	ret = 0;
out_dput:
	dput(dentry);
	return ret;
}