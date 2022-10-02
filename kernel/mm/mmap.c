#include <mm/mmap.h>
#include <mm/mm.h>
#include <proc/sched.h>
#include <stderr.h>
#include <fcntl.h>

/*
 * Find previous memory region.
 */
static struct vm_area_t *find_vma_prev(uint32_t addr)
{
	struct vm_area_t *vm, *vm_prev = NULL;
	struct list_head_t *pos;

	list_for_each(pos, &current_task->vm_list) {
		vm = list_entry(pos, struct vm_area_t, list);
		if (addr < vm->vm_end)
			break;

		vm_prev = vm;
	}

	return vm_prev;
}

/*
 * Find a memory region.
 */
static struct vm_area_t *find_vma(uint32_t addr)
{
	struct list_head_t *pos;
	struct vm_area_t *vm;

	list_for_each(pos, &current_task->vm_list) {
		vm = list_entry(pos, struct vm_area_t, list);
		if (addr < vm->vm_start)
			break;
		if (addr < vm->vm_end)
			return vm;
	}

	return NULL;
}

/*
 * Generic mmap.
 */
static struct vm_area_t *generic_mmap(uint32_t addr, size_t len, int flags, struct file_t *filp, off_t offset)
{
	struct vm_area_t *vm, *vm_prev;
	size_t f_pos;
	int ret;

	/* create new memory region */
	vm = (struct vm_area_t *) kmalloc(sizeof(struct vm_area_t));
	if (!vm)
		return NULL;

	/* set new memory region */
	vm->vm_start = addr;
	vm->vm_end = addr + len;
	vm->vm_flags = flags;

	/* unmap existing pages */
	unmap_pages(vm->vm_start, vm->vm_end, current_task->pgd);

	/* map pages */
	ret = map_pages(vm->vm_start, vm->vm_end, current_task->pgd, 0, 1);
	if (ret)
		goto err;

	/* memzero new memory region */
	memset((void *) vm->vm_start, 0, vm->vm_end - vm->vm_start);

	/* add it to the list */
	vm_prev = find_vma_prev(vm->vm_start);
	if (vm_prev)
		list_add(&vm->list, &vm_prev->list);
	else
		list_add(&vm->list, &current_task->vm_list);

	/* map file */
	if (filp) {
		f_pos = filp->f_pos;

		if (generic_lseek(filp, offset, SEEK_SET) < 0)
			goto err;

		if (filp->f_op->read(filp, (char *) vm->vm_start, len) < 0)
			goto err;

		filp->f_pos = f_pos;
	}

	return vm;
err:
	unmap_pages(vm->vm_start, vm->vm_end, current_task->pgd);
	kfree(vm);
	return NULL;
}

/*
 * Memory map system call.
 */
void *do_mmap(uint32_t addr, size_t len, int flags, struct file_t *filp, off_t offset)
{
	struct vm_area_t *vm = NULL;
	struct list_head_t *pos;

	/* adjust length */
	len = PAGE_ALIGN_UP(len);

	/* get address */
	if (flags & MAP_FIXED) {
		if (addr & ~PAGE_MASK)
			return NULL;
	} else {
		addr = UMAP_START;

		/* find last memory region */
		list_for_each(pos, &current_task->vm_list) {
			vm = list_entry(pos, struct vm_area_t, list);
			if (vm->vm_end >= addr)
				addr = vm->vm_end;
		}

		/* check memory map overflow */
		if (addr >= UMAP_END)
			return NULL;
	}

	/* create a new area */
	vm = generic_mmap(addr, len, flags, filp, offset);
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
	vm = find_vma(addr);
	if (!vm || addr > vm->vm_start)
		return 0;

	/* shrink memory region */
	if (vm->vm_end - vm->vm_start > len) {
		vm->vm_end = addr;
		return 0;
	}

	/* free memory region */
	unmap_pages(vm->vm_start, vm->vm_end, current_task->pgd);
	list_del(&vm->list);
	kfree(vm);

	return 0;
}
