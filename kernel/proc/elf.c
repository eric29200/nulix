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
 * Load an ELF file in memory.
 */
int elf_load(const char *path)
{
	struct elf_prog_header_t *ph, *last_ph = NULL;
	struct elf_header_t *elf_header;
	char name[TASK_NAME_LEN], *buf;
	struct list_head_t *pos, *n;
	struct vm_area_t *vm;
	int fd, off, ret;
	void *load_addr;
	uint32_t i;

	/* open file */
	fd = do_open(AT_FDCWD, path, O_RDONLY, 0);
	if (fd < 0)
		return fd;

	/* save path */
	strncpy(name, path, TASK_NAME_LEN - 1);
	name[TASK_NAME_LEN - 1] = 0;

	/* allocate a buffer */
	buf = (char *) kmalloc(PAGE_SIZE);
	if (!buf) {
		ret = -ENOMEM;
		goto out;
	}

	/* read first block */
	if ((size_t) do_read(fd, buf, PAGE_SIZE) < sizeof(struct elf_header_t)) {
		ret = -EINVAL;
		goto out;
	}

	/* check elf header */
	elf_header = (struct elf_header_t *) buf;
	ret = elf_check(elf_header);
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
	current_task->user_entry = elf_header->e_entry;
	current_task->user_stack = USTACK_START;

	/* read each elf segment */
	for (i = 0, off = elf_header->e_phoff; i < elf_header->e_phnum; i++, off += sizeof(struct elf_prog_header_t)) {
		/* load segment */
		ph = (struct elf_prog_header_t *) (buf + off);
		if (ph->p_type == PT_LOAD) {
			/* map elf segment */
			load_addr = do_mmap(ph->p_vaddr & PAGE_MASK,
					    ph->p_filesz + (ph->p_vaddr & (PAGE_SIZE - 1)),
					    MAP_FIXED,
					    current_task->filp[fd],
					    ph->p_offset & PAGE_MASK);
			if (!load_addr) {
				ret = -ENOMEM;
				goto out;
			}

			/* remember last segment */
			last_ph = ph;
		}
	}

	/* no segment */
	if (!last_ph) {
		ret = -ENOEXEC;
		goto out;
	}

	/* setup BSS section */
	memset((void *) (last_ph->p_vaddr + last_ph->p_filesz), 0, last_ph->p_memsz - last_ph->p_filesz);

	/* setup HEAP section */
	current_task->end_text = last_ph->p_vaddr + last_ph->p_memsz;
	current_task->start_brk = PAGE_ALIGN_UP(current_task->end_text);
	current_task->end_brk = PAGE_ALIGN_UP(current_task->end_text);

	/* change task name */
	memcpy(current_task->name, name, TASK_NAME_LEN);

	ret = 0;
out:
	do_close(fd);
	kfree(buf);
	return ret;
}
