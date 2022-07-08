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
static struct vm_area_t *get_unmaped_area(int flags, size_t length)
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
	vm->vm_flags = flags;

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
 * Expand a memory region.
 */
static struct vm_area_t *expand_area(struct vm_area_t *vm, uint32_t addr)
{
	struct vm_area_t *vm_next;

	/* no need to expand */
	if (vm->vm_end >= addr)
		return vm;

	/* last memory region : just expand */
	if (list_is_last(&vm->list, &current_task->vm_list)) {
		vm->vm_end = PAGE_ALIGN_UP(addr);
		return vm;
	}

	/* no intersection with next memory region : just expand */
	vm_next = list_next_entry(vm, list);
	if (addr <= vm_next->vm_start) {
		vm->vm_end = PAGE_ALIGN_UP(addr);
		return vm;
	}

	/* create new memory area */
	vm_next = get_unmaped_area(vm->vm_flags, addr - vm->vm_start);
	if (!vm_next)
		return NULL;

	/* delete old memory region */
	list_del(&vm->list);
	kfree(vm);

	return vm_next;
}

/*
 * Memory map system call.
 */
void *do_mmap(uint32_t addr, size_t length, int flags)
{
	struct vm_area_t *vm;

	/* check if addr is already mapped in a region */
	vm = find_vma(addr);

	/* expand area or create a new one */
	if (vm)
		vm = expand_area(vm, addr + length);
	else
		vm = get_unmaped_area(flags, length);

	/* no way to map memory */
	if (!vm)
		return NULL;

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
	if (addr + length < vm->vm_end) {
		vm->vm_end = addr;
	} else {
		list_del(&vm->list);
		kfree(vm);
	}

	return 0;
}
