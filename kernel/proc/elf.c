#include <proc/elf.h>
#include <proc/sched.h>
#include <mm/mm.h>
#include <fs/fs.h>
#include <sys/syscall.h>
#include <fcntl.h>
#include <stderr.h>
#include <stdio.h>
#include <string.h>

#define ELF_MIN_ALIGN		PAGE_SIZE
#define ELF_PAGESTART(_v)	((_v) & ~(uint32_t)(ELF_MIN_ALIGN - 1))
#define ELF_PAGEOFFSET(_v)	((_v) & (ELF_MIN_ALIGN - 1))
#define ELF_PAGEALIGN(_v)	(((_v) + ELF_MIN_ALIGN - 1) & ~(ELF_MIN_ALIGN - 1))

#define DLINFO_ITEMS		12

/*
 * Check header file
 */
static int elf_check(struct elf_header *elf_header)
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

	if (elf_header->e_type != ET_EXEC
	    && elf_header->e_type != ET_DYN)
		return -ENOEXEC;

	return 0;
}

/*
 * Create ELF table.
 */
static int elf_create_tables(struct binargs *bargs, uint32_t *sp, char *args_str, struct elf_header *elf_header,
			     uint32_t load_addr, uint32_t load_bias, uint32_t interp_load_addr)
{
	int i;

	/* put string arguments at the end of the stack */
	memcpy((void *) args_str, bargs->buf, bargs->argv_len + bargs->envp_len);

	/* put argc */
	*sp++ = bargs->argc;

	/* put argv */
	current_task->mm->arg_start = (uint32_t) sp;
	for (i = 0; i < bargs->argc; i++) {
		*sp++ = (uint32_t) args_str;
		args_str += strlen((char *) args_str) + 1;
	}

	/* finish argv with NULL pointer */
	current_task->mm->arg_end = (uint32_t) sp;
	*sp++ = 0;

	/* put envp */
	current_task->mm->env_start = (uint32_t) sp;
	for (i = 0; i < bargs->envc; i++) {
		*sp++ = (uint32_t) args_str;
		args_str += strlen((char *) args_str) + 1;
	}

	/* finish envp with NULL pointer */
	current_task->mm->env_end = (uint32_t) sp;
	*sp++ = 0;

#define AUX_ENT(id, val)	*sp++ = id; *sp++ = val;
	AUX_ENT(AT_PAGESZ, PAGE_SIZE);
	AUX_ENT(AT_PHDR, load_addr + elf_header->e_phoff);
	AUX_ENT(AT_PHENT, sizeof(struct elf_prog_header));
	AUX_ENT(AT_PHNUM, elf_header->e_phnum);
	AUX_ENT(AT_BASE, interp_load_addr);
	AUX_ENT(AT_FLAGS, 0);
	AUX_ENT(AT_ENTRY, load_bias + elf_header->e_entry);
	AUX_ENT(AT_UID, current_task->uid);
	AUX_ENT(AT_EUID, current_task->euid);
	AUX_ENT(AT_GID, current_task->gid);
	AUX_ENT(AT_EGID, current_task->egid);
#undef AUX_ENT

	return 0;
}

/*
 * Load an ELF interpreter in memory.
 */
int elf_load_interpreter(const char *path, uint32_t *interp_load_addr, uint32_t *elf_entry)
{
	int fd, ret, off, elf_type, elf_prot, load_addr_set = 0;
	struct elf_prog_header *ph, *last_ph = NULL;
	uint32_t i, load_addr = 0, start, end;
	struct elf_header *elf_header;
	char *buf, *buf_mmap;
	struct file *filp;

	/* open file */
	fd = do_open(AT_FDCWD, path, O_RDONLY, 0);
	if (fd < 0)
		return fd;

	/* get file */
	filp = current_task->files->filp[fd];

	/* get a free page */
	buf = (char *) get_free_page();
	if (!buf) {
		ret = -ENOMEM;
		goto out;
	}

	/* read first block */
	if ((size_t) do_read(filp, buf, PAGE_SIZE, &filp->f_pos) < sizeof(struct elf_header)) {
		ret = -EINVAL;
		goto out;
	}

	/* check elf header */
	elf_header = (struct elf_header *) buf;
	ret = elf_check(elf_header);
	if (ret != 0)
		goto out;

	/* read each elf segment */
	for (i = 0, off = elf_header->e_phoff; i < elf_header->e_phnum; i++, off += sizeof(struct elf_prog_header)) {
		/* load segment */
		ph = (struct elf_prog_header *) (buf + off);
		if (ph->p_type == PT_LOAD) {
			elf_type = MAP_PRIVATE;
			elf_prot = 0;

			/* set mmap protocol */
			if (ph->p_flags & FLAG_READ)
				elf_prot |= PROT_READ;
			if (ph->p_flags & FLAG_WRITE)
				elf_prot |= PROT_WRITE;
			if (ph->p_flags & FLAG_READ)
				elf_prot |= PROT_EXEC;

			/* set mmap type */
			if (elf_header->e_type == ET_EXEC || load_addr_set)
				elf_type |= MAP_FIXED;

			/* map elf segment */
			buf_mmap = do_mmap(ELF_PAGESTART(ph->p_vaddr + load_addr),
					   ph->p_filesz + ELF_PAGEOFFSET(ph->p_vaddr),
					   elf_prot,
					   elf_type,
					   filp,
					   ph->p_offset - ELF_PAGEOFFSET(ph->p_vaddr));
			if (!buf_mmap) {
				ret = -ENOMEM;
				goto out;
			}

			/* set load address */
			if (!load_addr_set && elf_header->e_type == ET_DYN) {
				load_addr = (uint32_t) buf_mmap - ELF_PAGESTART(ph->p_vaddr);
				load_addr_set = 1;
			}

			/* remember last header header */
			last_ph = ph;
		}
	}

	/* no segment */
	if (!last_ph) {
		ret = -ENOEXEC;
		goto out;
	}

	/* apply load bias */
	last_ph->p_vaddr += load_addr;
	*interp_load_addr = load_addr;
	*elf_entry = load_addr + elf_header->e_entry;

	/* memzero fractionnal page of data section */
	start = last_ph->p_vaddr + last_ph->p_filesz;
	end = PAGE_ALIGN_UP(start);
	memset((void *) start, 0, end - start);

	/* setup BSS section */
	start = PAGE_ALIGN_UP(last_ph->p_vaddr + last_ph->p_filesz);
	end = PAGE_ALIGN_UP(last_ph->p_vaddr + last_ph->p_memsz);
	if (!do_mmap(start, end - start, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_FIXED, NULL, 0)) {
		ret = -ENOMEM;
		goto out;
	}

	ret = 0;
out:
	sys_close(fd);
	free_page(buf);
	return ret;
}

/*
 * Clear current executable (clear memory, signals and files).
 */
static int clear_old_exec()
{
	struct signal_struct *sig_new = NULL, *sig_old;
	struct mm_struct *mm_new;
	int fd, i, ret;

	/* make sig private */
	sig_old = current_task->sig;
	if (current_task->sig->count > 1) {
		/* allocate a private sig */
		sig_new = (struct signal_struct *) kmalloc(sizeof(struct signal_struct));
		if (!sig_new) {
			ret = -ENOMEM;
			goto err_sig;
		}

		/* copy actions */
		sig_new->count = 1;
		memcpy(sig_new->action, current_task->sig->action, sizeof(sig_new->action));
		current_task->sig = sig_new;
	}


	/* clear all memory regions */
	if (current_task->mm->count == 1) {
		task_release_mmap(current_task);
		task_exit_mmap(current_task->mm);
	} else {
		/* duplicate mm struct */
		mm_new = task_dup_mm(current_task->mm);
		if (!mm_new) {
			ret = -ENOMEM;
			goto err_mm;
		}

		/* decrement old mm count and set new mm struct */
		current_task->mm->count--;
		current_task->mm = mm_new;

		/* switch to new page directory */
		switch_pgd(current_task->mm->pgd);

		/* release mapping */
		task_release_mmap(current_task);
	}

	/* release old signals */
	if (current_task->sig != sig_old)
		sig_old->count--;

	/* clear signal handlers */
	for (i = 0; i < NSIGS; i++) {
		if (current_task->sig->action[i].sa_handler != SIG_IGN)
			current_task->sig->action[i].sa_handler = SIG_DFL;

		current_task->sig->action[i].sa_flags = 0;
		sigemptyset(&current_task->sig->action[i].sa_mask);
	}

	/* close files marked close on exec */
	for (fd = 0; fd < NR_OPEN; fd++) {
		if (FD_ISSET(fd, &current_task->files->close_on_exec)) {
			sys_close(fd);
			FD_CLR(fd, &current_task->files->close_on_exec);
		}
	}

	return 0;
err_mm:
	if (sig_new)
		kfree(sig_new);
err_sig:
	current_task->sig = sig_old;
	return ret;
}

/*
 * Load an ELF file in memory.
 */
int elf_load(const char *path, struct binargs *bargs)
{
	uint32_t start, end, i, sp, args_str, load_addr = 0, load_bias = 0, interp_load_addr = 0, elf_entry;
	char name[TASK_NAME_LEN], *buf = NULL, *elf_interpreter = NULL;
	int fd, off, ret, elf_type, elf_prot, load_addr_set = 0;
	struct elf_prog_header *ph, *last_ph = NULL;
	struct elf_header *elf_header;
	struct file *filp = NULL;
	void *buf_mmap;

	/* open file */
	fd = do_open(AT_FDCWD, path, O_RDONLY, 0);
	if (fd < 0)
		return fd;

	/* get file */
	filp = current_task->files->filp[fd];

	/* check permissions */
	ret = permission(filp->f_dentry->d_inode, MAY_EXEC);
	if (ret)
		goto out;

	/* save path */
	strncpy(name, path, TASK_NAME_LEN - 1);
	name[TASK_NAME_LEN - 1] = 0;

	/* get a free page */
	buf = (char *) get_free_page();
	if (!buf) {
		ret = -ENOMEM;
		goto out;
	}

	/* read first block */
	if ((size_t) do_read(filp, buf, PAGE_SIZE, &filp->f_pos) < sizeof(struct elf_header)) {
		ret = -EINVAL;
		goto out;
	}

	/* check elf header */
	elf_header = (struct elf_header *) buf;
	ret = elf_check(elf_header);
	if (ret != 0)
		goto out;

	/* clear current executable */
	ret = clear_old_exec();
	if (ret)
		goto out;

	/* reset code */
	current_task->mm->start_text = 0;
	current_task->mm->end_text = 0;
	elf_entry = elf_header->e_entry;

	/* check if an ELF interpreter is needed */
	for (i = 0, off = elf_header->e_phoff; i < elf_header->e_phnum; i++, off += sizeof(struct elf_prog_header)) {
		/* load segment */
		ph = (struct elf_prog_header *) (buf + off);
		if (ph->p_type == PT_INTERP) {
			/* allocate interpreter path */
			elf_interpreter = (char *) kmalloc(ph->p_filesz);
			if (!elf_interpreter) {
				ret = -ENOMEM;
				goto out;
			}

			/* seek to interpreter path */
			ret = do_llseek(filp, ph->p_offset, SEEK_SET);
			if (ret < 0)
				goto out;

			/* read interpreter path */
			if ((size_t) do_read(filp, elf_interpreter, ph->p_filesz, &filp->f_pos) != ph->p_filesz) {
				ret = -EINVAL;
				goto out;
			}

			break;
		}
	}

	/* read each elf segment */
	for (i = 0, off = elf_header->e_phoff; i < elf_header->e_phnum; i++, off += sizeof(struct elf_prog_header)) {
		/* load segment */
		ph = (struct elf_prog_header *) (buf + off);
		if (ph->p_type == PT_LOAD) {
			elf_type = MAP_PRIVATE;
			elf_prot = 0;

			/* set mmap protocol */
			if (ph->p_flags & FLAG_READ)
				elf_prot |= PROT_READ;
			if (ph->p_flags & FLAG_WRITE)
				elf_prot |= PROT_WRITE;
			if (ph->p_flags & FLAG_READ)
				elf_prot |= PROT_EXEC;

			/* set mmap type */
			if (elf_header->e_type == ET_EXEC || load_addr_set)
				elf_type |= MAP_FIXED;
			else if (elf_header->e_type == ET_DYN)
				load_bias = ELF_PAGESTART(ELF_ET_DYN_BASE - ph->p_vaddr);

			/* map elf segment */
			buf_mmap = do_mmap(ELF_PAGESTART(ph->p_vaddr + load_bias),
					   ph->p_filesz + ELF_PAGEOFFSET(ph->p_vaddr),
					   elf_prot,
					   elf_type,
					   filp,
					   ph->p_offset - ELF_PAGEOFFSET(ph->p_vaddr));
			if (!buf_mmap) {
				ret = -ENOMEM;
				goto out;
			}

			/* set load address */
			if (!load_addr_set) {
				load_addr_set = 1;
				load_addr = ph->p_vaddr - ph->p_offset;
				if (elf_header->e_type == ET_DYN) {
					load_bias += (uint32_t) buf_mmap - ELF_PAGESTART(load_bias + ph->p_vaddr);
					load_addr += load_bias;
				}
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

	/* apply load bias */
	elf_entry += load_bias;
	last_ph->p_vaddr += load_bias;

	/* memzero fractionnal page of data section */
	start = last_ph->p_vaddr + last_ph->p_filesz;
	end = PAGE_ALIGN_UP(start);
	memset((void *) start, 0, end - start);

	/* setup BSS section */
	start = PAGE_ALIGN_UP(last_ph->p_vaddr + last_ph->p_filesz);
	end = PAGE_ALIGN_UP(last_ph->p_vaddr + last_ph->p_memsz);
	if (!do_mmap(start, end - start, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_FIXED, NULL, 0)) {
		ret = -ENOMEM;
		goto out;
	}

	/* setup HEAP section */
	current_task->mm->end_text = PAGE_ALIGN_UP(last_ph->p_vaddr + last_ph->p_memsz);
	current_task->mm->start_brk = current_task->mm->end_text;
	current_task->mm->end_brk = current_task->mm->end_text + PAGE_SIZE;
	if (!do_mmap(current_task->mm->start_brk, current_task->mm->end_brk - current_task->mm->start_brk,
		     PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_FIXED, NULL, 0)) {
		ret = -ENOMEM;
		goto out;
	}

	/* load ELF interpreter */
	if (elf_interpreter) {
		ret = elf_load_interpreter(elf_interpreter, &interp_load_addr, &elf_entry);
		if (ret)
			goto out;
	}

	/* setup stack */
	sp = USTACK_START;
	sp -= bargs->argv_len + bargs->envp_len;
	args_str = sp;
	sp -= 2 * DLINFO_ITEMS * sizeof(uint32_t);
	sp -= (1 + (bargs->argc + 1) + (bargs->envc + 1)) * sizeof(uint32_t);

	/* map initial stack */
	start = PAGE_ALIGN_DOWN(sp);
	end = USTACK_START;
	if (!do_mmap(start, end - start, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_FIXED | VM_GROWSDOWN, NULL, 0)) {
		ret = -ENOMEM;
		goto out;
	}

	/* create ELF tables */
	ret = elf_create_tables(bargs, (uint32_t *) sp, (char *) args_str, elf_header, load_addr, load_bias, interp_load_addr);
	if (ret)
		goto out;

	/* change task name */
	memcpy(current_task->name, name, TASK_NAME_LEN);

	/* setup task entry and stack pointer */
	current_task->thread.regs.eip = elf_entry;
	current_task->thread.regs.useresp = sp;
out:
	sys_close(fd);
	if (elf_interpreter)
		kfree(elf_interpreter);
	if (buf)
		free_page(buf);
	return ret;
}
