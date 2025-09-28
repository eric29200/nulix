#include <mm/mmap.h>
#include <mm/mm.h>
#include <proc/sched.h>
#include <stdio.h>
#include <stderr.h>
#include <fcntl.h>

/*
 * Page protection.
 */
uint32_t protection_map[16] = {
	__P000, __P001, __P010, __P011, __P100, __P101, __P110, __P111,
	__S000, __S001, __S010, __S011, __S100, __S101, __S110, __S111
};

/*
 * Find a memory region.
 */
struct vm_area *find_vma(struct task *task, uint32_t addr)
{
	struct list_head *pos;
	struct vm_area *vma;

	list_for_each(pos, &task->mm->vm_list) {
		vma = list_entry(pos, struct vm_area, list);
		if (addr < vma->vm_start)
			break;
		if (addr < vma->vm_end)
			return vma;
	}

	return NULL;
}

/*
 * Find previous memory region.
 */
struct vm_area *find_vma_prev(struct task *task, uint32_t addr)
{
	struct vm_area *vma, *vma_prev = NULL;
	struct list_head *pos;

	list_for_each(pos, &task->mm->vm_list) {
		vma = list_entry(pos, struct vm_area, list);
		if (addr < vma->vm_end)
			break;

		vma_prev = vma;
	}

	return vma_prev;
}

/*
 * Find next memory region.
 */
struct vm_area *find_vma_next(struct task *task, uint32_t addr)
{
	struct list_head *pos;
	struct vm_area *vma;

	list_for_each(pos, &task->mm->vm_list) {
		vma = list_entry(pos, struct vm_area, list);
		if (addr < vma->vm_start)
			return vma;
	}

	return NULL;
}
/*
 * Find first vm intersecting start <-> end.
 */
struct vm_area *find_vma_intersection(struct task *task, uint32_t start, uint32_t end)
{
	struct vm_area *vma_prev, *vm_next = NULL;

	/* find previous and next vma */
	vma_prev = find_vma_prev(task, start);
	if (vma_prev)
		vm_next = list_next_entry_or_null(vma_prev, &task->mm->vm_list, list);
	else if (!list_empty(&task->mm->vm_list))
		vm_next = list_first_entry(&task->mm->vm_list, struct vm_area, list);

	/* check next vma */
	if (vm_next && end > vm_next->vm_start)
		return vm_next;

	return NULL;
}
  
/*
 * Move a memory region.
 */
static uint32_t move_vma(struct vm_area *vma, uint32_t old_address, size_t old_size, uint32_t new_address, size_t new_size)
{
	struct vm_area *vma_new, *vma_prev;

	/* create new memory region */
	vma_new = (struct vm_area *) kmalloc(sizeof(struct vm_area));
	if (!vma_new)
		return -ENOMEM;

	/* set new memory region */
	*vma_new = *vma;
	vma_new->vm_start = new_address;
	vma_new->vm_end = new_address + new_size;
	if (vma_new->vm_file)
		vma_new->vm_file->f_count++;
	if (vma_new->vm_file && vma_new->vm_ops && vma_new->vm_ops->open)
		vma_new->vm_ops->open(vma_new);

	/* unmap existing pages */
	do_munmap(new_address, new_size);

	/* add it to the list */
	vma_prev = find_vma_prev(current_task, vma_new->vm_start);
	if (vma_prev)
		list_add(&vma_new->list, &vma_prev->list);
	else
		list_add(&vma_new->list, &current_task->mm->vm_list);

	/* unmap old region */
	do_munmap(old_address, old_size);

	return vma_new->vm_start;
}

/*
 * Get an unmapped area.
 */
static int get_unmapped_area(uint32_t *addr, size_t len, int flags)
{
	struct vm_area *vma, *vma_prev, *vm_next = NULL;
	struct list_head *pos;

	/* fixed address */
	if (flags & MAP_FIXED) {
		if (*addr & ~PAGE_MASK)
			return -EINVAL;
		return 0;
	}

	/* try to use addr */
	if (*addr) {
		*addr = PAGE_ALIGN_UP(*addr);

		/* find previous and next vm */
		vma_prev = find_vma_prev(current_task, *addr);
		if (vma_prev)
			vm_next = list_next_entry_or_null(vma_prev, &current_task->mm->vm_list, list);
		else if (!list_empty(&current_task->mm->vm_list))
			vm_next = list_first_entry(&current_task->mm->vm_list, struct vm_area, list);

		/* addr is available */
		if (!vm_next || *addr + len <= vm_next->vm_start)
			return 0;
	}

	/* find a memory region */
	*addr = UMAP_START;
	list_for_each(pos, &current_task->mm->vm_list) {
		vma = list_entry(pos, struct vm_area, list);
		if (*addr + len <= vma->vm_start)
			break;
		if (vma->vm_end > *addr)
			*addr = vma->vm_end;
	}

	/* check memory map overflow */
	if (*addr + len >= UMAP_END)
		return -ENOMEM;

	return 0;
}

/*
 * Memory map system call.
 */
void *do_mmap(uint32_t addr, size_t len, int prot, int flags, struct file *filp, off_t offset)
{
	struct vm_area *vma, *vma_prev;
	int ret;

	/* check flags */
	if (!filp && (flags & MAP_TYPE) != MAP_PRIVATE)
		return NULL;

	/* check if mmap is implemented */
	if (filp && (!filp->f_op || !filp->f_op->mmap))
		return NULL;

	/* adjust length */
	len = PAGE_ALIGN_UP(len);
	if (len == 0)
		return (void *) addr;

	/* get unmapped area */
	if (get_unmapped_area(&addr, len, flags))
		return NULL;

	/* create new memory region */
	vma = (struct vm_area *) kmalloc(sizeof(struct vm_area));
	if (!vma)
		return NULL;

	/* set new memory region */
	memset(vma, 0, sizeof(struct vm_area));
	vma->vm_start = addr;
	vma->vm_end = addr + len;
	vma->vm_flags = prot & (VM_READ | VM_WRITE | VM_EXEC);
	vma->vm_flags |= flags & (VM_GROWSDOWN | VM_DENYWRITE | VM_EXECUTABLE);
	vma->vm_page_prot = protection_map[vma->vm_flags & 0x0F];
	vma->vm_offset = offset;
	vma->vm_file = NULL;
	vma->vm_ops = NULL;
	vma->vm_mm = current_task->mm;

	/* unmap existing pages */
	do_munmap(addr, len);

	/* map file */
	if (filp) {
		/* shared mapping */
		if (flags & MAP_SHARED)
			vma->vm_flags |= VM_SHARED;

		/* check inode */
		if (!filp->f_dentry || !filp->f_dentry->d_inode)
			goto err;

		/* mmap file */
		ret = filp->f_op->mmap(filp, vma);
		if (ret)
			goto err;

		/* update file reference count */
		vma->vm_file = filp;
		filp->f_count++;

		/* add memory region to inode */
		list_add_tail(&vma->list_share, &filp->f_dentry->d_inode->i_mmap);
	}

	/* add it to the list */
	vma_prev = find_vma_prev(current_task, vma->vm_start);
	if (vma_prev)
		list_add(&vma->list, &vma_prev->list);
	else
		list_add(&vma->list, &current_task->mm->vm_list);

	return (void *) vma->vm_start;
err:
	kfree(vma);
	return NULL;
}

/*
 * Unmap a region (create a hole if needed).
 */
static int unmap_fixup(struct vm_area *vma, uint32_t addr, size_t len)
{
	uint32_t end = addr + len;
	struct vm_area *vma_new;

	/* unmap the whole area */
	if (addr == vma->vm_start && end == vma->vm_end) {
		if (vma->vm_ops && vma->vm_ops->close)
			vma->vm_ops->close(vma);
		if (vma->vm_file)
			fput(vma->vm_file);

		list_del(&vma->list);
		kfree(vma);
		return 0;
	}

	/* shrink area or create a hole */
	if (end == vma->vm_end) {
		vma->vm_end = addr;
	} else if (addr == vma->vm_start) {
		vma->vm_offset += (end - vma->vm_start);
		vma->vm_start = end;
	} else {
		/* create new memory region */
		vma_new = (struct vm_area *) kmalloc(sizeof(struct vm_area));
		if (!vma_new)
			return -ENOMEM;

		/* set new memory region = after the hole */
		memset(vma_new, 0, sizeof(struct vm_area));
		vma_new->vm_start = end;
		vma_new->vm_end = vma->vm_end;
		vma_new->vm_flags = vma->vm_flags;
		vma_new->vm_page_prot = vma->vm_page_prot;
		vma_new->vm_offset = vma->vm_offset + (end - vma->vm_start);
		vma_new->vm_file = vma->vm_file;
		vma_new->vm_ops = vma->vm_ops;
		vma_new->vm_mm = vma->vm_mm;

		/* open region */
		if (vma_new->vm_file)
			vma_new->vm_file->f_count++;
		if (vma_new->vm_ops && vma_new->vm_ops->open)
			vma_new->vm_ops->open(vma_new);

		/* add new memory region after old one */
		list_add(&vma_new->list, &vma->list);

		/* update old memory region */
		vma->vm_end = addr;
	}

	return 0;
}

/*
 * Memory unmap system call.
 */
int do_munmap(uint32_t addr, size_t len)
{
	struct list_head *pos, *n;
	uint32_t start, end, nr;
	struct vm_area *vma;

	/* add must be page aligned */
	if (addr & ~PAGE_MASK)
		return -EINVAL;

	/* adjust length */
	len = PAGE_ALIGN_UP(len);

	/* find regions to unmap */
	list_for_each_safe(pos, n, &current_task->mm->vm_list) {
		vma = list_entry(pos, struct vm_area, list);
		if (addr >= vma->vm_end)
			continue;
		if (addr + len <= vma->vm_start)
			break;

		/* compute area to unmap */
		start = addr < vma->vm_start ? vma->vm_start : addr;
		end = addr + len > vma->vm_end ? vma->vm_end : addr + len;

		/* unmap */
		if (vma->vm_ops && vma->vm_ops->unmap)
			vma->vm_ops->unmap(vma, start, end - start);

		/* unmap it */
		unmap_fixup(vma, start, end - start);
	}

	/* unmap region */
	nr = zap_page_range(current_task->mm->pgd, addr, len);

	/* update resident memory size */
	if (nr >= current_task->mm->rss)
		current_task->mm->rss -= nr;
	else
		current_task->mm->rss = 0;

	return 0;
}

/*
 * Memory region remap system call.
 */
static uint32_t do_mremap(uint32_t old_address, size_t old_size, size_t new_size, int flags, uint32_t new_address)
{
	struct vm_area *vma, *vma_next;
	int ret = -EINVAL;
	int map_flags;

	/* check flags */
	if (flags & ~(MREMAP_FIXED | MREMAP_MAYMOVE))
		goto err;

	/* old address must be page aligned */
	if (old_address & ~PAGE_MASK)
		goto err;

	/* page align lengths */
	old_size = PAGE_ALIGN_UP(old_size);
	new_size = PAGE_ALIGN_UP(new_size);

	if (flags & MREMAP_FIXED) {
		/* new address must be page aligned and movable */
		if (new_address & ~PAGE_MASK)
			goto err;
		if (!(flags & MREMAP_MAYMOVE))
			goto err;

		/* allow new_size == 0 only if new_size == old_size */
		if (!new_size && new_size != old_size)
			goto err;

		/* new memory region must not overlap old memory region */
		if (new_address <= old_address && new_address + new_size > old_address)
			goto err;
		if (old_address <= new_address && old_address + old_size > new_address)
			goto err;

		/* unmap new region */
		ret = do_munmap(new_address, new_size);
		if (ret && new_size)
			goto err;
	}

	/* shrinking remap */
	if (old_size >= new_size) {
		/* unmap pages */
		ret = do_munmap(old_address + new_size, old_size - new_size);
		if (ret && old_size != new_size)
			goto err;

		/* done */
		if (!(flags & MREMAP_FIXED) || new_address == old_address)
			return old_address;
	}

	/* find old memory region */
	ret = -EFAULT;
	vma = find_vma(current_task, old_address);
	if (!vma || vma->vm_start > old_address)
		goto err;

	/* check old memory region size */
	if (old_size > vma->vm_end - old_address)
		goto err;

	/* try to grow old region */
	if (old_size == vma->vm_end - old_address
		&& !(flags & MREMAP_FIXED)
		&& new_address != old_address
		&& (old_size != new_size || !(flags & MREMAP_MAYMOVE))) {
			/* get next vma */
			vma_next = list_next_entry_or_null(vma, &current_task->mm->vm_list, list);

			/* just expand old region */
			if (!vma_next || vma_next->vm_start - old_address >= new_size) {
				vma->vm_end = old_address + new_size;
				return old_address;
			}
	}

	/* unable to shrink or exapand the area : create a new one */
	if (flags & MREMAP_MAYMOVE) {
		if (!(flags & MREMAP_FIXED)) {
			map_flags = 0;
			if (vma->vm_flags & VM_SHARED)
				map_flags |= MAP_SHARED;

			ret = get_unmapped_area(&new_address, new_size, map_flags);
			if (ret)
				goto err;
		}

		return move_vma(vma, old_address, old_size, new_address, new_size);
	}

err:
	return ret;
}

/*
 * Change memory protection.
 */
static int mprotect_fixup_all(struct vm_area *vma, uint16_t newflags, uint32_t newprot)
{
	vma->vm_flags = newflags;
	vma->vm_page_prot = newprot;
	return 0;
}

/*
 * Change memory protection (start of vma).
 */
static int mprotect_fixup_start(struct vm_area *vma, uint32_t end, uint16_t newflags, uint32_t newprot)
{
	struct vm_area *vma_new;

	/* create new memory region */
	vma_new = (struct vm_area *) kmalloc(sizeof(struct vm_area));
	if (!vma_new)
		return -ENOMEM;

	/* set new memory region */
	*vma_new = *vma;
	if (vma_new->vm_file)
		vma_new->vm_file->f_count++;
	vma_new->vm_flags = newflags;
	vma_new->vm_page_prot = newprot;

	/* split region */
	vma_new->vm_end = end;
	vma->vm_start = end;
	vma->vm_offset += vma->vm_start - vma_new->vm_start;

	/* add new memory region before old one */
	list_add_tail(&vma_new->list, &vma->list);

	return 0;
}

/*
 * Change memory protection (end of vma).
 */
static int mprotect_fixup_end(struct vm_area *vma, uint32_t start, uint16_t newflags, uint32_t newprot)
{
	struct vm_area *vma_new;

	/* create new memory region */
	vma_new = (struct vm_area *) kmalloc(sizeof(struct vm_area));
	if (!vma_new)
		return -ENOMEM;

	/* set new memory region */
	*vma_new = *vma;
	if (vma_new->vm_file)
		vma_new->vm_file->f_count++;
	vma_new->vm_flags = newflags;
	vma_new->vm_page_prot = newprot;

	/* split region */
	vma->vm_end = start;
	vma_new->vm_start = start;
	vma_new->vm_offset += vma_new->vm_start - vma->vm_start;

	/* add new memory region after old one */
	list_add(&vma_new->list, &vma->list);

	return 0;
}

/*
 * Change memory protection (middle of vma).
 */
static int mprotect_fixup_middle(struct vm_area *vma, uint32_t start, uint32_t end, uint16_t newflags, uint32_t newprot)
{
	UNUSED(vma);
	UNUSED(start);
	UNUSED(end);
	UNUSED(newflags);
	UNUSED(newprot);
	printf("mprotect_fixup_middle() not implemented\n");
	return -EINVAL;
}

/*
 * Change memory protection (split memory region if needed).
 */
static int mprotect_fixup(struct vm_area *vma, uint32_t start, uint32_t end, uint16_t newflags)
{
	uint32_t newprot;
	int ret;

	/* nothing to do */
	if (vma->vm_flags == newflags)
		return 0;

	/* get new protection */
	newprot = protection_map[newflags & 0x0F];

	/* fix and split */
	if (vma->vm_start == start && vma->vm_end == end)
		ret = mprotect_fixup_all(vma, newflags, newprot);
	else if (vma->vm_start == start)
		ret = mprotect_fixup_start(vma, end, newflags, newprot);
	else if (vma->vm_end == end)
		ret = mprotect_fixup_end(vma, start, newflags, newprot);
	else
		ret = mprotect_fixup_middle(vma, start, end, newflags, newprot);

	return ret;
}

/*
 * Memory region protect system call.
 */
int do_mprotect(uint32_t start, size_t size, int prot)
{
	struct vm_area *vma, *next;
	uint32_t nstart, end, tmp;
	uint16_t newflags;
	int ret = 0;

	/* address must be page aligned */
	if (start & ~PAGE_MASK)
		return -EINVAL;

	/* page align length */
	size = PAGE_ALIGN_UP(size);

	/* check protocol */
	if (prot & ~(PROT_READ | PROT_WRITE | PROT_EXEC))
		return -EINVAL;

	/* compute end address */
	end = start + size;
	if (end < start)
		return -EINVAL;

	/* empty region */
	if (end == start)
		return 0;

	/* find first memory region */
	vma = find_vma(current_task, start);
	if (!vma)
		return -EFAULT;

	/* change memory regions */
	for (nstart = start;;) {
		/* compute new flags */
		newflags = prot | (vma->vm_flags & ~(PROT_READ | PROT_WRITE | PROT_EXEC));

		/* last memory region */
		if (vma->vm_end >= end) {
			ret = mprotect_fixup(vma, nstart, end, newflags);
			break;
		}

		/* protect memory region */
		tmp = vma->vm_end;
		next = list_next_entry_or_null(vma, &current_task->mm->vm_list, list);
		ret = mprotect_fixup(vma, nstart, tmp, newflags);
		if (ret)
			break;

		/* go to next memory region */
		nstart = tmp;
		vma = next;

		/* hole or no more regions */
		if (!vma || vma->vm_start != nstart) {
			ret = -EFAULT;
			break;
		}
	}

	return ret;
}

/*
 * Truncate memory regions.
 */
void vmtruncate(struct inode *inode, off_t offset)
{
	UNUSED(offset);

	if (!list_empty(&inode->i_mmap))
		printf("vmtruncate() not implemented");
}

/*
 * Old mmap system call.
 */
int old_mmap(struct mmap_arg_struct *arg)
{
	struct file *filp = NULL;
	void *ret;

	/* offset must be page aligned */
	if (arg->offset & ~PAGE_MASK)
		return -EINVAL;

	/* get file */
	if (!(arg->flags & MAP_ANONYMOUS)) {
		filp = fget(arg->fd);
		if (!filp)
			return -EBADF;
	}

	/* do mmap */
	ret = do_mmap(arg->addr, arg->len, arg->prot, arg->flags, filp, arg->offset);

	/* release file */
	if (filp)
		fput(filp);

	return (int) ret;
}

/*
 * Memory map 2 system call.
 */
int sys_mmap2(uint32_t addr, size_t length, int prot, int flags, int fd, off_t pgoffset)
{
	struct file *filp = NULL;
	void *ret;

	/* get file */
	if (fd >= 0) {
		filp = fget(fd);
		if (!filp)
			return -EBADF;
	}

	/* do mmap */
	ret = do_mmap(addr, length, prot, flags, filp, pgoffset * PAGE_SIZE);

	/* release file */
	if (filp)
		fput(filp);

	return (int) ret;
}

/*
 * Memory unmap system call.
 */
int sys_munmap(uint32_t addr, size_t length)
{
	return do_munmap(addr, length);
}

/*
 * Memory region remap system call.
 */
int sys_mremap(uint32_t old_address, size_t old_size, size_t new_size, int flags, uint32_t new_address)
{
	return do_mremap(old_address, old_size, new_size, flags, new_address);
}

/*
 * Madvise system call.
 */
int sys_madvise(uint32_t addr, size_t length, int advice)
{
	UNUSED(addr);
	UNUSED(length);
	UNUSED(advice);
	return 0;
}

/*
 * Mprotect system call.
 */
int sys_mprotect(uint32_t addr, size_t len, int prot)
{
	return do_mprotect(addr, len, prot);
}

/*
 * Change data segment end address.
 */
uint32_t sys_brk(uint32_t brk)
{
	uint32_t newbrk, oldbrk;

	/* current brk is asked */
	if (brk < current_task->mm->end_code)
		goto out;

	/* grow brk, without new page */
	newbrk = PAGE_ALIGN_UP(brk);
	oldbrk = PAGE_ALIGN_UP(current_task->mm->end_brk);
	if (oldbrk == newbrk)
		goto set_brk;

	/* shrink brk */
	if (brk <= current_task->mm->end_brk) {
		if (do_munmap(newbrk, oldbrk - newbrk) == 0)
			goto set_brk;
		goto out;
	}

	/* check against existing mapping */
	if (find_vma_intersection(current_task, oldbrk, newbrk + PAGE_SIZE))
		goto out;

	/* map new pages */
	if (do_mmap(oldbrk, newbrk - oldbrk, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_FIXED, NULL, 0) != (void *) oldbrk)
		goto out;

set_brk:
	current_task->mm->end_brk = brk;
out:
	return current_task->mm->end_brk;
}

/*
 * Determine wether pages are resident in memory.
 */
int sys_mincore(uint32_t addr, size_t len, unsigned char *vec)
{
	UNUSED(addr);
	UNUSED(len);
	UNUSED(vec);
	return 0;
}
