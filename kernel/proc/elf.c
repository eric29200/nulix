#include <proc/elf.h>
#include <proc/sched.h>
#include <mm/mm.h>
#include <fs/fs.h>
#include <fcntl.h>
#include <stderr.h>
#include <string.h>

/*
 * Check header file
 */
static int elf_check(struct elf_header_t *elf_header)
{
	if (elf_header->e_ident[EI_MAG0] != ELFMAG0
	    || elf_header->e_ident[EI_MAG1] != ELFMAG1
	    || elf_header->e_ident[EI_MAG2] != ELFMAG2
	    || elf_header->e_ident[EI_MAG3] != ELFMAG3)
		return -ENOEXEC;

	if (elf_header->e_ident[EI_CLASS] != ELFCLASS32)
		return -ENOEXEC;

	if (elf_header->e_ident[EI_DATA] != ELFDATA2LSB)
		return -ENOEXEC;

	if (elf_header->e_ident[EI_VERSION] != EV_CURRENT)
		return -ENOEXEC;

	if (elf_header->e_machine != EM_386)
		return -ENOEXEC;

	if (elf_header->e_type != ET_EXEC)
		return -ENOEXEC;

	return 0;
}

/*
 * Load an ELF segment in memory.
 */
static int segment_load(int fd, int off)
{
	struct elf_prog_header_t ph;
	int ret;

	/* seek to prog header */
	ret = do_lseek(fd, off, SEEK_SET);
	if (ret < 0)
		return ret;

	/* read prog header */
	if (do_read(fd, (void *) &ph, sizeof(struct elf_prog_header_t)) != sizeof(struct elf_prog_header_t))
		return -ENOSPC;

	/* set start text position */
	if (ph.p_vaddr > 0 && current_task->start_text == 0)
		current_task->start_text = 0;

	/* update end text position */
	if (ph.p_vaddr + ph.p_filesz > current_task->end_text)
		current_task->end_text = ph.p_vaddr + ph.p_filesz;

	/* skip non relocable segments */
	if (ph.p_type != ET_REL)
		return 0;

	/* map pages */
	if (do_mmap(ph.p_vaddr, PAGE_ALIGN_UP(ph.p_filesz), 0) == NULL)
		return -ENOMEM;

	/* seek to elf segment */
	ret = do_lseek(fd, ph.p_offset, SEEK_SET);
	if (ret < 0)
		return ret;

	/* load segment in memory */
	memset((void *) ph.p_vaddr, 0, ph.p_memsz);
	if (do_read(fd, (void *) ph.p_vaddr, ph.p_filesz) != (int) ph.p_filesz)
		return -ENOSPC;

	return 0;
}

/*
 * Load an ELF file in memory.
 */
int elf_load(const char *path)
{
	struct elf_header_t elf_header;
	struct list_head_t *pos, *n;
	char name[TASK_NAME_LEN];
	struct vm_area_t *vm;
	int fd, off, ret;
	uint32_t i;

	/* open file */
	fd = do_open(AT_FDCWD, path, O_RDONLY, 0);
	if (fd < 0)
		return fd;

	/* save path */
	strncpy(name, path, TASK_NAME_LEN - 1);
	name[TASK_NAME_LEN - 1] = 0;

	/* read elf header */
	if ((size_t) do_read(fd, (void *) &elf_header, sizeof(struct elf_header_t)) != sizeof(struct elf_header_t)) {
		do_close(fd);
		return -EINVAL;
	}

	/* check elf header */
	ret = elf_check(&elf_header);
	if (ret != 0)
		goto out;

	/* clear mapping */
	list_for_each_safe(pos, n, &current_task->vm_list) {
		vm = list_entry(pos, struct vm_area_t, list);
		list_del(&vm->list);
		kfree(vm);
	}

	/* reset text/brk section */
	current_task->start_text = 0;
	current_task->end_text = 0;
	current_task->start_brk = 0;
	current_task->end_brk = 0;

	/* read each elf segment */
	for (i = 0, off = elf_header.e_phoff; i < elf_header.e_phnum; i++, off += sizeof(struct elf_prog_header_t)) {
		ret = segment_load(fd, off);
		if (ret != 0)
			goto out;
	}

 	/* set data segment */
	current_task->start_brk = PAGE_ALIGN_UP(current_task->end_text) + PAGE_SIZE;
	current_task->end_brk = PAGE_ALIGN_UP(current_task->end_text) + PAGE_SIZE;

	/* allocate at the end of process memory */
	if (do_mmap(USTACK_START - USTACK_SIZE, USTACK_SIZE, 0) == NULL) {
		ret = -ENOMEM;
		goto out;
	}

	/* set elf entry point */
	current_task->user_entry = elf_header.e_entry;
	current_task->user_stack = USTACK_START;

	/* change task name */
	memcpy(current_task->name, name, TASK_NAME_LEN);

	ret = 0;
out:
	do_close(fd);
	return ret;
}
