#include <mm/mmap.h>
#include <mm/mm.h>
#include <proc/sched.h>
#include <stderr.h>

/*
 * Find a memory region.
 */
static struct vm_area_t *find_vma(uint32_t addr)
{
	struct list_head_t *pos;
	struct vm_area_t *vm;

	list_for_each(pos, &current_task->vm_list) {
		vm = list_entry(pos, struct vm_area_t, list);
		if (addr >= vm->vm_start && addr < vm->vm_end)
			return vm;
	}

	return NULL;
}

/*
 * Get an unmapped area.
 */
static struct vm_area_t *get_unmaped_area(size_t length)
{
	struct vm_area_t *vm_prev, *vm;

	/* find last memory region */
	if (list_empty(&current_task->vm_list))
		vm_prev = NULL;
	else
		vm_prev = list_last_entry(&current_task->vm_list, struct vm_area_t, list);

	/* create new memory region */
	vm = (struct vm_area_t *) kmalloc(sizeof(struct vm_area_t));
	if (!vm)
		return NULL;

	/* set new memory region */
	vm->vm_start = vm_prev ? vm_prev->vm_end : UMAP_START;
	vm->vm_end = PAGE_ALIGN_UP(vm->vm_start + length);

	/* check memory map overflow */
	if (vm->vm_end > UMAP_END) {
		kfree(vm);
		return NULL;
	}

	/* add it to the list */
	list_add_tail(&vm->list, &current_task->vm_list);

	return vm;
}

/*
 * Memory map system call.
 */
void *do_mmap(uint32_t addr, size_t length, int flags, struct file_t *filp)
{
	struct vm_area_t *vm = NULL;
	struct list_head_t *pos;
	size_t f_pos;

	/* unused address */
	UNUSED(addr);

	/* try to get a free region */
	list_for_each(pos, &current_task->vm_list) {
		vm = list_entry(pos, struct vm_area_t, list);
		if (vm->vm_free && vm->vm_end - vm->vm_start >= length)
			goto found;
	}

	/* create a new area */
	vm = get_unmaped_area(length);
	if (!vm)
		return NULL;

found:
	/* memzero new memory region */
	vm->vm_flags = flags;
	vm->vm_free = 0;
	memset((void *) vm->vm_start, 0, vm->vm_end - vm->vm_start);

	/* map file */
	if (filp) {
		f_pos = filp->f_pos;
		filp->f_op->read(filp, (char *) vm->vm_start, length);
		filp->f_pos = f_pos;
	}

	return (void *) vm->vm_start;
}

/*
 * Memory unmap system call.
 */
int do_munmap(uint32_t addr, size_t length)
{
	struct vm_area_t *vm;

	/* find memory region */
	vm = find_vma(addr);

	/* addr does not match start of memory region : break */
	if (!vm || addr > vm->vm_start)
		return 0;

	/* shrink or free memory region */
	if (vm->vm_end - vm->vm_start > PAGE_ALIGN_UP(length))
		vm->vm_end = addr;
	else
		vm->vm_free = 1;

	return 0;
}
