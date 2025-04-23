#include <x86/ldt.h>
#include <x86/gdt.h>
#include <x86/segment.h>
#include <proc/sched.h>
#include <stdio.h>
#include <stderr.h>

/*
 * Set Local Descriptor Table.
 */
static void set_ldt(void *addr, uint32_t entries)
{
	struct ldttss_desc ldt;

	/* clear LDT */
	if (entries == 0) {
		__asm__ __volatile__("lldt %w0"::"q" (0));
		return;
	}

	/* set LDT descriptor */
	set_tssldt_descriptor(&ldt, (uint32_t) addr, DESC_LDT, entries * LDT_ENTRY_SIZE - 1);

	/* write it to GDT */
	gdt_write_entry(GDT_ENTRY_LDT, (struct desc_struct *) &ldt);

	/* load it */
	__asm__ __volatile__("lldt %w0"::"q" (GDT_ENTRY_LDT * 8));
}

/*
 * Load Local Descriptor Table.
 */
static void load_ldt(struct task *task)
{
	set_ldt(task->mm->ldt, task->mm->ldt_size);
}

/*
 * Switch Local Descriptor Table.
 */
void switch_ldt(struct task *prev, struct task *next)
{
	if ((prev && prev->mm->ldt) || next->mm->ldt)
		load_ldt(next);
}

/*
 * Clone a Local Descriptor Table.
 */
int clone_ldt(struct mm_struct *src, struct mm_struct *dst)
{
	/* allocate a new LDT */
	dst->ldt = (struct desc_struct *) kmalloc(sizeof(struct desc_struct) * src->ldt_size);
	if (!dst->ldt)
		return -ENOMEM;

	/* copy LDT */
	memcpy(dst->ldt, src->ldt, sizeof(struct desc_struct) * src->ldt_size);
	dst->ldt_size = src->ldt_size;

	return 0;
}

/*
 * Allocate a new Local Descriptor Table.
 */
static int alloc_ldt(size_t min_count)
{
	void *new_ldt, *old_ldt;
	size_t old_size;

	old_size = current_task->mm->ldt_size;
	if (min_count <= old_size)
		return 0;

	/* allocate a new LDT */
	new_ldt = kmalloc(min_count * LDT_ENTRY_SIZE);
	if (!new_ldt)
		return -ENOMEM;

	/* copy old LDT */
	if (old_size)
		memcpy(new_ldt, current_task->mm->ldt, old_size * LDT_ENTRY_SIZE);

	/* clear new LDTs */
	old_ldt = current_task->mm->ldt;
	memset(new_ldt + old_size * LDT_ENTRY_SIZE, 0, (min_count - old_size) * LDT_ENTRY_SIZE);

	/* load new LDT */
	current_task->mm->ldt = new_ldt;
	current_task->mm->ldt_size = min_count;
	load_ldt(current_task);

	/* free old LDT */
	if (old_size)
		kfree(old_ldt);

	return 0;
}

/*
 * Write a Local Descriptor Table.
 */
static int write_ldt(void *ptr, uint32_t bytecount, int old_mode)
{
	struct user_desc *ldt_info = (struct user_desc *) ptr;
	struct desc_struct ldt;
	int ret;

	/* check input paramater */
	if (bytecount != sizeof(struct user_desc))
		return -EINVAL;

	/* check entry number */
	if (ldt_info->entry_number >= LDT_ENTRIES)
		return -EINVAL;

	/* check parameters */
	if (ldt_info->contents == 3) {
		if (old_mode)
			return -EINVAL;
		if (ldt_info->seg_not_present == 0)
			return -EINVAL;
	}

	/* allocate a new LDT if needed */
	if (ldt_info->entry_number >= current_task->mm->ldt_size) {
		ret = alloc_ldt(ldt_info->entry_number + 1);
		if (ret < 0)
			return ret;
	}

	/* allow LDTs to be cleared by the user */
	if (ldt_info->base_addr == 0 && ldt_info->limit == 0) {
		if (old_mode || LDT_empty(ldt_info)) {
			memset(&ldt, 0, sizeof(ldt));
			goto install;
		}
	}

	/* fill LDT */
	fill_ldt(&ldt, ldt_info);
	if (old_mode)
		ldt.avl = 0;

	/* finally install LDT */
install:
	memcpy(&current_task->mm->ldt[ldt_info->entry_number], &ldt, 8);
	return 0;
}

/*
 * Read default LDT = zero.
 */
static int read_default_ldt(void *ptr, uint32_t bytecount)
{
	memcpy(ptr, 0, bytecount);
	return bytecount;
}

/*
 * Read a Local Descriptor Table.
 */
static int read_ldt(void *ptr, uint32_t bytecount)
{
	size_t entries_size;

	/* no LDT */
	if (!current_task->mm->ldt)
		return 0;

	/* limit bytecount */
	if (bytecount > LDT_ENTRY_SIZE * LDT_ENTRIES)
		bytecount = LDT_ENTRY_SIZE * LDT_ENTRIES;

	/* number of entries to read */
	entries_size = current_task->mm->ldt_size * LDT_ENTRIES;
	if (entries_size > bytecount)
		entries_size = bytecount;

	/* copy entries */
	memcpy(ptr, current_task->mm->ldt, entries_size);

	/* zero fill remaining buffer */
	if (entries_size != bytecount)
		memset(ptr + entries_size, 0, bytecount - entries_size);

	return bytecount;
}

/*
 * Modify Local Descriptor Table system call.
 */
int sys_modify_ldt(int func, void *ptr, uint32_t bytecount)
{
	switch (func) {
		case 0:
			return read_ldt(ptr, bytecount);
		case 1:
			return write_ldt(ptr, bytecount, 1);
		case 2:
			return read_default_ldt(ptr, bytecount);
		case 0x11:
			return write_ldt(ptr, bytecount, 0);
		default:
			return -ENOSYS;
	}

	return 0;
}
