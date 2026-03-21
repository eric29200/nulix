#include <proc/sched.h>
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
 * Try to swap out a page.
 */
static int try_to_swap_out(struct vm_area *vma, uint32_t address, pte_t *pte)
{
	struct page *page;

	/* page not present */
	if (!pte_present(*pte))
		return 0;

	/* non valid page */
	page = pte_page(*pte);
	if (!VALID_PAGE(page))
		return 0;

	if (!page->inode || PageReserved(page))
		return 0;

	/* clean page : drop pte */
	if (!pte_dirty(*pte))
		goto drop_pte;

	/* swapout not implemented */
	if (!vma->vm_ops || !vma->vm_ops->swapout)
		return 0;

	/* swap out page */
	vma->vm_ops->swapout(vma, page);
drop_pte:
	/* drop pte and free page */
	pte_clear(pte);
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
	struct swap_info *p;
	uint32_t type;
	int ret, page;

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

	/* swap file must be a regular file */
	ret = -EINVAL;
	if (!S_ISREG(swap_dentry->d_inode->i_mode))
		goto err;

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
	if (p->swap_map)
		kfree(p->swap_map);
	if (p->swap_file)
		dput(p->swap_file);
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