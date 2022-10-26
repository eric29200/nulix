#include <mm/mmap.h>
#include <mm/mm.h>
#include <proc/sched.h>
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
	unmap_pages(vm->vm_start, vm->vm_end, current_task->mm->pgd);

	/* add it to the list */
	vm_prev = find_vma_prev(current_task, vm->vm_start);
	if (vm_prev)
		list_add(&vm->list, &vm_prev->list);
	else
		list_add(&vm->list, &current_task->mm->vm_list);

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
 * Memory unmap system call.
 */
int do_munmap(uint32_t addr, size_t len)
{
	struct vm_area_t *vm;

	/* add must be page aligned */
	if (addr & ~PAGE_MASK)
		return -EINVAL;

	/* adjust length */
	len = PAGE_ALIGN_UP(len);

	/* find memory region */
	vm = find_vma(current_task, addr);
	if (!vm || addr > vm->vm_start)
		return 0;

	/* shrink memory region */
	if (vm->vm_end - vm->vm_start > len) {
		vm->vm_end = addr;
		return 0;
	}

	/* free memory region */
	unmap_pages(vm->vm_start, vm->vm_end, current_task->mm->pgd);
	list_del(&vm->list);
	kfree(vm);

	return 0;
}
