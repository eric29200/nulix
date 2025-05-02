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
	struct vm_area *vm;

	list_for_each(pos, &task->mm->vm_list) {
		vm = list_entry(pos, struct vm_area, list);
		if (addr < vm->vm_start)
			break;
		if (addr < vm->vm_end)
			return vm;
	}

	return NULL;
}

/*
 * Find previous memory region.
 */
struct vm_area *find_vma_prev(struct task *task, uint32_t addr)
{
	struct vm_area *vm, *vm_prev = NULL;
	struct list_head *pos;

	list_for_each(pos, &task->mm->vm_list) {
		vm = list_entry(pos, struct vm_area, list);
		if (addr < vm->vm_end)
			break;

		vm_prev = vm;
	}

	return vm_prev;
}

/*
 * Find next memory region.
 */
struct vm_area *find_vma_next(struct task *task, uint32_t addr)
{
	struct list_head *pos;
	struct vm_area *vm;

	list_for_each(pos, &task->mm->vm_list) {
		vm = list_entry(pos, struct vm_area, list);
		if (addr < vm->vm_start)
			return vm;
	}

	return NULL;
}
/*
 * Find first vm intersecting start <-> end.
 */
struct vm_area *find_vma_intersection(struct task *task, uint32_t start, uint32_t end)
{
	struct vm_area *vm_prev, *vm_next = NULL;

	/* find previous and next vma */
	vm_prev = find_vma_prev(task, start);
	if (vm_prev)
		vm_next = list_next_entry_or_null(vm_prev, &task->mm->vm_list, list);
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
void *move_vma(struct vm_area *vm, uint32_t old_address, size_t old_size, uint32_t new_address, size_t new_size)
{
	struct vm_area *vm_new, *vm_prev;

	/* create new memory region */
	vm_new = (struct vm_area *) kmalloc(sizeof(struct vm_area));
	if (!vm_new)
		return NULL;

	/* set new memory region */
	*vm_new = *vm;
	vm_new->vm_start = new_address;
	vm_new->vm_end = new_address + new_size;
	if (vm_new->vm_inode && vm_new->vm_ops && vm_new->vm_ops->open)
		vm_new->vm_ops->open(vm_new);

	/* unmap existing pages */
	do_munmap(new_address, new_size);

	/* add it to the list */
	vm_prev = find_vma_prev(current_task, vm_new->vm_start);
	if (vm_prev)
		list_add(&vm_new->list, &vm_prev->list);
	else
		list_add(&vm_new->list, &current_task->mm->vm_list);

	/* unmap old region */
	do_munmap(old_address, old_size);

	return (void *) vm_new->vm_start;
}

/*
 * Generic mmap.
 */
static struct vm_area *generic_mmap(uint32_t addr, size_t len, int prot, int flags, struct file *filp, off_t offset)
{
	struct vm_area *vm, *vm_prev;
	int ret;

	/* create new memory region */
	vm = (struct vm_area *) kmalloc(sizeof(struct vm_area));
	if (!vm)
		return NULL;

	/* set new memory region */
	memset(vm, 0, sizeof(struct vm_area));
	vm->vm_start = addr;
	vm->vm_end = addr + len;
	vm->vm_flags = prot & (VM_READ | VM_WRITE | VM_EXEC);
	vm->vm_flags |= flags & (VM_GROWSDOWN | VM_DENYWRITE | VM_EXECUTABLE);
	vm->vm_page_prot = protection_map[vm->vm_flags & 0x0F];
	vm->vm_offset = offset;
	vm->vm_inode = NULL;
	vm->vm_ops = NULL;

	/* unmap existing pages */
	do_munmap(addr, len);

	/* map file */
	if (filp) {
		/* shared mapping */
		if (flags & MAP_SHARED)
			vm->vm_flags |= VM_SHARED;

		/* mmap file */
		ret = filp->f_op->mmap(filp->f_inode, vm);
		if (ret)
			goto err;

		/* add memory region to inode */
		list_add_tail(&vm->list_share, &filp->f_inode->i_mmap);
	}

	/* add it to the list */
	vm_prev = find_vma_prev(current_task, vm->vm_start);
	if (vm_prev)
		list_add(&vm->list, &vm_prev->list);
	else
		list_add(&vm->list, &current_task->mm->vm_list);

	return vm;
err:
	kfree(vm);
	return NULL;
}

/*
 * Get an unmapped area.
 */
static int get_unmapped_area(uint32_t *addr, size_t len, int flags)
{
	struct vm_area *vm, *vm_prev, *vm_next = NULL;
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
		vm_prev = find_vma_prev(current_task, *addr);
		if (vm_prev)
			vm_next = list_next_entry_or_null(vm_prev, &current_task->mm->vm_list, list);
		else if (!list_empty(&current_task->mm->vm_list))
			vm_next = list_first_entry(&current_task->mm->vm_list, struct vm_area, list);

		/* addr is available */
		if (!vm_next || *addr + len <= vm_next->vm_start)
			return 0;
	}

	/* find a memory region */
	*addr = UMAP_START;
	list_for_each(pos, &current_task->mm->vm_list) {
		vm = list_entry(pos, struct vm_area, list);
		if (*addr + len <= vm->vm_start)
			break;
		if (vm->vm_end > *addr)
			*addr = vm->vm_end;
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
	struct vm_area *vm = NULL;

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

	/* create a new area */
	vm = generic_mmap(addr, len, prot, flags, filp, offset);
	if (!vm)
		return NULL;

	return (void *) vm->vm_start;
}

/*
 * Unmap a region (create a hole if needed).
 */
static int unmap_fixup(struct vm_area *vm, uint32_t addr, size_t len)
{
	uint32_t end = addr + len;
	struct vm_area *vm_new;

	/* unmap the whole area */
	if (addr == vm->vm_start && end == vm->vm_end) {
		if (vm->vm_ops && vm->vm_ops->close)
			vm->vm_ops->close(vm);

		list_del(&vm->list);
		kfree(vm);
		return 0;
	}

	/* shrink area or create a hole */
	if (end == vm->vm_end) {
		vm->vm_end = addr;
	} else if (addr == vm->vm_start) {
		vm->vm_offset += (end - vm->vm_start);
		vm->vm_start = end;
	} else {
		/* create new memory region */
		vm_new = (struct vm_area *) kmalloc(sizeof(struct vm_area));
		if (!vm_new)
			return -ENOMEM;

		/* set new memory region = after the hole */
		memset(vm_new, 0, sizeof(struct vm_area));
		vm_new->vm_start = end;
		vm_new->vm_end = vm->vm_end;
		vm_new->vm_flags = vm->vm_flags;
		vm_new->vm_page_prot = vm->vm_page_prot;
		vm_new->vm_offset = vm->vm_offset + (end - vm->vm_start);
		vm_new->vm_inode = vm->vm_inode;
		vm_new->vm_ops = vm->vm_ops;

		/* open region */
		if (vm_new->vm_ops && vm_new->vm_ops->open)
			vm_new->vm_ops->open(vm_new);

		/* add new memory region after old one */
		list_add(&vm_new->list, &vm->list);

		/* update old memory region */
		vm->vm_end = addr;
	}

	return 0;
}

/*
 * Memory unmap system call.
 */
int do_munmap(uint32_t addr, size_t len)
{
	struct list_head *pos, *n;
	struct vm_area *vm;
	uint32_t start, end;

	/* add must be page aligned */
	if (addr & ~PAGE_MASK)
		return -EINVAL;

	/* adjust length */
	len = PAGE_ALIGN_UP(len);

	/* find regions to unmap */
	list_for_each_safe(pos, n, &current_task->mm->vm_list) {
		vm = list_entry(pos, struct vm_area, list);
		if (addr >= vm->vm_end)
			continue;
		if (addr + len <= vm->vm_start)
			break;

		/* compute area to unmap */
		start = addr < vm->vm_start ? vm->vm_start : addr;
		end = addr + len > vm->vm_end ? vm->vm_end : addr + len;

		/* unmap it */
		unmap_fixup(vm, start, end - start);
	}

	/* unmap region */
	unmap_pages(addr, addr + len, current_task->mm->pgd);

	return 0;
}

/*
 * Memory region remap system call.
 */
void *do_mremap(uint32_t old_address, size_t old_size, size_t new_size, int flags, uint32_t new_address)
{
	struct vm_area *vma, *vma_next;
	int ret, map_flags;

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
			return (void *) old_address;
	}

	/* find old memory region */
	vma = find_vma(current_task, old_address);
	if (!vma || vma->vm_start > old_address)
		return NULL;

	/* check old memory region size */
	if (old_size > vma->vm_end - old_address)
		return NULL;

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
				return (void *) old_address;
			}
	}

	/* unable to shrink or exapand the area : create a new one */
	if (flags & MREMAP_MAYMOVE) {
		if (!(flags & MREMAP_FIXED)) {
			map_flags = 0;
			if (vma->vm_flags & VM_SHARED)
				map_flags |= MAP_SHARED;

			if (get_unmapped_area(&new_address, new_size, map_flags))
				goto err;
		}

		return move_vma(vma, old_address, old_size, new_address, new_size);
	}

err:
	return NULL;
}

/*
 * Change memory protection.
 */
static int mprotect_fixup_all(struct vm_area *vm, uint16_t newflags, uint32_t newprot)
{
	vm->vm_flags = newflags;
	vm->vm_page_prot = newprot;
	return 0;
}

/*
 * Change memory protection (start of vm).
 */
static int mprotect_fixup_start(struct vm_area *vm, uint32_t end, uint16_t newflags, uint32_t newprot)
{
	struct vm_area *vm_new;

	/* create new memory region */
	vm_new = (struct vm_area *) kmalloc(sizeof(struct vm_area));
	if (!vm_new)
		return -ENOMEM;

	/* set new memory region */
	*vm_new = *vm;
	if (vm_new->vm_inode)
		vm_new->vm_inode->i_count++;
	vm_new->vm_flags = newflags;
	vm_new->vm_page_prot = newprot;

	/* split region */
	vm_new->vm_end = end;
	vm->vm_start = end;
	vm->vm_offset += vm->vm_start - vm_new->vm_start;

	/* add new memory region before old one */
	list_add_tail(&vm_new->list, &vm->list);

	return 0;
}

/*
 * Change memory protection (end of vm).
 */
static int mprotect_fixup_end(struct vm_area *vm, uint32_t start, uint16_t newflags, uint32_t newprot)
{
	struct vm_area *vm_new;

	/* create new memory region */
	vm_new = (struct vm_area *) kmalloc(sizeof(struct vm_area));
	if (!vm_new)
		return -ENOMEM;

	/* set new memory region */
	*vm_new = *vm;
	if (vm_new->vm_inode)
		vm_new->vm_inode->i_count++;
	vm_new->vm_flags = newflags;
	vm_new->vm_page_prot = newprot;

	/* split region */
	vm->vm_end = start;
	vm_new->vm_start = start;
	vm_new->vm_offset += vm_new->vm_start - vm->vm_start;

	/* add new memory region after old one */
	list_add(&vm_new->list, &vm->list);

	return 0;
}

/*
 * Change memory protection (middle of vm).
 */
static int mprotect_fixup_middle(struct vm_area *vm, uint32_t start, uint32_t end, uint16_t newflags, uint32_t newprot)
{
	UNUSED(vm);
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
static int mprotect_fixup(struct vm_area *vm, uint32_t start, uint32_t end, uint16_t newflags)
{
	uint32_t newprot;
	int ret;

	/* nothing to do */
	if (vm->vm_flags == newflags)
		return 0;

	/* get new protection */
	newprot = protection_map[newflags & 0x0F];

	/* fix and split */
	if (vm->vm_start == start && vm->vm_end == end)
		ret = mprotect_fixup_all(vm, newflags, newprot);
	else if (vm->vm_start == start)
		ret = mprotect_fixup_start(vm, end, newflags, newprot);
	else if (vm->vm_end == end)
		ret = mprotect_fixup_end(vm, start, newflags, newprot);
	else
		ret = mprotect_fixup_middle(vm, start, end, newflags, newprot);

	return ret;
}

/*
 * Memory region protect system call.
 */
int do_mprotect(uint32_t start, size_t size, int prot)
{
	struct vm_area *vm, *next;
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
	vm = find_vma(current_task, start);
	if (!vm)
		return -EFAULT;

	/* change memory regions */
	for (nstart = start;;) {
		/* compute new flags */
		newflags = prot | (vm->vm_flags & ~(PROT_READ | PROT_WRITE | PROT_EXEC));

		/* last memory region */
		if (vm->vm_end >= end) {
			ret = mprotect_fixup(vm, nstart, end, newflags);
			break;
		}

		/* protect memory region */
		tmp = vm->vm_end;
		next = list_next_entry_or_null(vm, &current_task->mm->vm_list, list);
		ret = mprotect_fixup(vm, nstart, tmp, newflags);
		if (ret)
			break;

		/* go to next memory region */
		nstart = tmp;
		vm = next;

		/* hole or no more regions */
		if (!vm || vm->vm_start != nstart) {
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
 * Memory map system call.
 */
void *sys_mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset)
{
	struct file *filp = NULL;

	/* get file */
	if (fd >= 0) {
		if (fd >= NR_OPEN || !current_task->files->filp[fd])
			return NULL;

		filp = current_task->files->filp[fd];
	}

	return do_mmap((uint32_t) addr, length, prot, flags, filp, offset);
}

/*
 * Memory map 2 system call.
 */
void *sys_mmap2(void *addr, size_t length, int prot, int flags, int fd, off_t pgoffset)
{
	struct file *filp = NULL;

	/* get file */
	if (fd >= 0) {
		if (fd >= NR_OPEN || !current_task->files->filp[fd])
			return NULL;

		filp = current_task->files->filp[fd];
	}

	return do_mmap((uint32_t) addr, length, prot, flags, filp, pgoffset * PAGE_SIZE);
}

/*
 * Memory unmap system call.
 */
int sys_munmap(void *addr, size_t length)
{
	return do_munmap((uint32_t) addr, length);
}

/*
 * Memory region remap system call.
 */
void *sys_mremap(void *old_address, size_t old_size, size_t new_size, int flags, void *new_address)
{
	return do_mremap((uint32_t) old_address, old_size, new_size, flags, (uint32_t) new_address);
}

/*
 * Madvise system call.
 */
int sys_madvise(void *addr, size_t length, int advice)
{
	UNUSED(addr);
	UNUSED(length);
	UNUSED(advice);
	return 0;
}

/*
 * Mprotect system call.
 */
int sys_mprotect(void *addr, size_t len, int prot)
{
	return do_mprotect((uint32_t) addr, len, prot);
}

/*
 * Change data segment end address.
 */
uint32_t sys_brk(uint32_t brk)
{
	uint32_t newbrk, oldbrk;

	/* current brk is asked */
	if (brk < current_task->mm->end_text)
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
int sys_mincore(void *addr, size_t len, unsigned char *vec)
{
	UNUSED(addr);
	UNUSED(len);
	UNUSED(vec);
	return 0;
}
