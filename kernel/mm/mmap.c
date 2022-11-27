#include <mm/mmap.h>
#include <mm/mm.h>
#include <proc/sched.h>
#include <stdio.h>
#include <stderr.h>
#include <fcntl.h>

/*
 * Find a memory region.
 */
struct vm_area_t *find_vma(struct task_t *task, uint32_t addr)
{
	struct list_head_t *pos;
	struct vm_area_t *vm;

	list_for_each(pos, &task->mm->vm_list) {
		vm = list_entry(pos, struct vm_area_t, list);
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
struct vm_area_t *find_vma_prev(struct task_t *task, uint32_t addr)
{
	struct vm_area_t *vm, *vm_prev = NULL;
	struct list_head_t *pos;

	list_for_each(pos, &task->mm->vm_list) {
		vm = list_entry(pos, struct vm_area_t, list);
		if (addr < vm->vm_end)
			break;

		vm_prev = vm;
	}

	return vm_prev;
}

/*
 * Find next memory region.
 */
struct vm_area_t *find_vma_next(struct task_t *task, uint32_t addr)
{
	struct list_head_t *pos;
	struct vm_area_t *vm;

	list_for_each(pos, &task->mm->vm_list) {
		vm = list_entry(pos, struct vm_area_t, list);
		if (addr < vm->vm_start)
			return vm;
	}

	return NULL;
}
/*
 * Find first vm intersecting start <-> end.
 */
struct vm_area_t *find_vma_intersection(struct task_t *task, uint32_t start, uint32_t end)
{
	struct vm_area_t *vm_prev, *vm_next = NULL;

	/* find previous and next vma */
	vm_prev = find_vma_prev(task, start);
	if (vm_prev)
		vm_next = list_next_entry_or_null(vm_prev, &task->mm->vm_list, list);
	else if (!list_empty(&task->mm->vm_list))
		vm_next = list_first_entry(&task->mm->vm_list, struct vm_area_t, list);

	/* check next vma */
	if (vm_next && end > vm_next->vm_start)
		return vm_next;

	return NULL;
}

/*
 * Generic mmap.
 */
static struct vm_area_t *generic_mmap(uint32_t addr, size_t len, int prot, int flags, struct file_t *filp, off_t offset)
{
	struct vm_area_t *vm, *vm_prev;
	int ret;

	/* create new memory region */
	vm = (struct vm_area_t *) kmalloc(sizeof(struct vm_area_t));
	if (!vm)
		return NULL;

	/* set new memory region */
	vm->vm_start = addr;
	vm->vm_end = addr + len;
	vm->vm_flags = prot & (VM_READ | VM_WRITE | VM_EXEC);
	vm->vm_flags |= flags & (VM_GROWSDOWN | VM_DENYWRITE | VM_EXECUTABLE);
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
	struct vm_area_t *vm, *vm_prev, *vm_next = NULL;
	struct list_head_t *pos;

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
			vm_next = list_first_entry(&current_task->mm->vm_list, struct vm_area_t, list);

		/* addr is available */
		if (!vm_next || *addr + len <= vm_next->vm_start)
			return 0;
	}

	/* find a memory region */
	*addr = UMAP_START;
	list_for_each(pos, &current_task->mm->vm_list) {
		vm = list_entry(pos, struct vm_area_t, list);
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
void *do_mmap(uint32_t addr, size_t len, int prot, int flags, struct file_t *filp, off_t offset)
{
	struct vm_area_t *vm = NULL;

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
static int unmap_fixup(struct vm_area_t *vm, uint32_t addr, size_t len)
{
	uint32_t end = addr + len;
	struct vm_area_t *vm_new;

	/* unmap the whole area */
	if (addr == vm->vm_start && end == vm->vm_end) {
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
		vm_new = (struct vm_area_t *) kmalloc(sizeof(struct vm_area_t));
		if (!vm_new)
			return -ENOMEM;

		/* set new memory region = after the hole */
		vm_new->vm_start = end;
		vm_new->vm_end = vm->vm_end;
		vm_new->vm_flags = vm->vm_flags;
		vm_new->vm_offset = vm->vm_offset + (end - vm->vm_start);
		vm_new->vm_inode = vm->vm_inode;
		if (vm_new->vm_inode)
			vm_new->vm_inode->i_ref++;
		vm_new->vm_ops = vm->vm_ops;

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
	struct list_head_t *pos, *n;
	struct vm_area_t *vm;
	uint32_t start, end;

	/* add must be page aligned */
	if (addr & ~PAGE_MASK)
		return -EINVAL;

	/* adjust length */
	len = PAGE_ALIGN_UP(len);

	/* find regions to unmap */
	list_for_each_safe(pos, n, &current_task->mm->vm_list) {
		vm = list_entry(pos, struct vm_area_t, list);
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
	struct vm_area_t *vma, *vma_next;
	int ret;

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

	/* TODO : move memory region into a new one */
	printf("mremap not implemented\n");
err:
	return NULL;
}