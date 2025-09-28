#include <proc/elf.h>
#include <proc/sched.h>
#include <proc/binfmt.h>
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
 * Setup BSS and BRK sections.
 */
static void set_brk(uint32_t start, uint32_t end)
{
	/* page align addresses */
	start = ELF_PAGEALIGN(start);
	end = ELF_PAGEALIGN(end);
	if (end <= start)
		return;

	/* map sections */
	do_mmap(NULL, start, end - start, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_FIXED | MAP_PRIVATE, 0);
}

/*
 * Setup stack.
 */
static void set_stack(struct binprm *bprm, uint32_t *sp, uint32_t *args_str)
{
	uint32_t start, end;

	/* setup stack */
	*sp = USTACK_START;
	*sp -= bprm->argv_len + bprm->envp_len;
	*args_str = *sp;
	*sp -= 2 * DLINFO_ITEMS * sizeof(uint32_t);
	*sp -= (1 + (bprm->argc + 1) + (bprm->envc + 1)) * sizeof(uint32_t);

	/* map initial stack */
	start = PAGE_ALIGN_DOWN(*sp);
	end = USTACK_START;
	do_mmap(NULL, start, end - start, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_FIXED | VM_GROWSDOWN, 0);
}

/*
 * Create ELF table.
 */
static int elf_create_tables(struct binprm *bprm, uint32_t *sp, char *args_str, struct elf_header *elf_header,
			     uint32_t load_addr, uint32_t load_bias, uint32_t interp_load_addr)
{
	int i;

	/* put string arguments at the end of the stack */
	memcpy((void *) args_str, bprm->buf_args, bprm->argv_len + bprm->envp_len);

	/* put argc */
	*sp++ = bprm->argc;

	/* put argv */
	current_task->mm->arg_start = (uint32_t) sp;
	for (i = 0; i < bprm->argc; i++) {
		*sp++ = (uint32_t) args_str;
		args_str += strlen((char *) args_str) + 1;
	}

	/* finish argv with NULL pointer */
	current_task->mm->arg_end = (uint32_t) sp;
	*sp++ = 0;

	/* put envp */
	current_task->mm->env_start = (uint32_t) sp;
	for (i = 0; i < bprm->envc; i++) {
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
static uint32_t elf_load_interpreter(struct elf_header *elf_header, struct dentry *interp_dentry, uint32_t *interp_load_addr)
{
	uint32_t i, load_addr = 0, map_addr, k, elf_bss = 0, last_bss = 0, ret = ~0UL;
	int fd, elf_type, elf_prot, load_addr_set = 0;
	struct elf_prog_header *ph, *first_ph = NULL;
	struct file *filp;
	size_t size;

	/* check elf header */
	if (elf_header->e_type != ET_EXEC && elf_header->e_type != ET_DYN)
		goto out;

	/* check elf header */
	if (elf_check(elf_header))
		goto out;

	/* open file */
	fd = open_dentry(interp_dentry, O_RDONLY);
	if (fd < 0)
		goto out;

	/* get file */
	filp = current_task->files->filp[fd];

	/* allocate program header */
	size = sizeof(struct elf_prog_header) * elf_header->e_phnum;
	first_ph = (struct elf_prog_header *) kmalloc(size);
	if (!first_ph) {
		ret = -ENOMEM;
		goto out;
	}

	/* read first program header */
	if (read_exec(interp_dentry, elf_header->e_phoff, (char *) first_ph, size) < 0)
		goto out;

	/* read each elf segment */
	for (i = 0, ph = first_ph; i < elf_header->e_phnum; i++, ph++) {
		/* skip non load segment */
		if (ph->p_type != PT_LOAD)
			continue;

		elf_type = MAP_PRIVATE | MAP_DENYWRITE;
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
		map_addr = (uint32_t) do_mmap(filp,
						ELF_PAGESTART(ph->p_vaddr + load_addr),
						ph->p_filesz + ELF_PAGEOFFSET(ph->p_vaddr),
						elf_prot,
						elf_type,
						ph->p_offset - ELF_PAGEOFFSET(ph->p_vaddr));
		if (!map_addr)
			goto out;

		/* set load address */
		if (!load_addr_set && elf_header->e_type == ET_DYN) {
			load_addr = map_addr - ELF_PAGESTART(ph->p_vaddr);
			load_addr_set = 1;
		}

		/* find the end of the file mapping */
		k = load_addr + ph->p_vaddr + ph->p_filesz;
		if (k > elf_bss)
			elf_bss = k;

		/* find the end of the memory mapping */
		k = load_addr + ph->p_vaddr + ph->p_memsz;
		if (k > last_bss)
			last_bss = k;
	}

	/* memzero fractionnal page of bss section */
	memset((void *) elf_bss, 0, PAGE_ALIGN_UP(elf_bss) - elf_bss);

	/* fill out BSS section */
	elf_bss = ELF_PAGESTART(elf_bss + PAGE_SIZE - 1);
	if (last_bss > elf_bss)
		do_mmap(NULL, elf_bss, last_bss - elf_bss, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_FIXED, 0);

	/* apply load bias */
	*interp_load_addr = load_addr;
	ret = elf_header->e_entry + load_addr;
out:
	sys_close(fd);
	kfree(first_ph);
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
static int elf_load_binary(struct binprm *bprm)
{
	uint32_t i, sp, args_str, load_addr = 0, load_bias = 0, interp_load_addr = 0, k;
	uint32_t elf_entry, elf_bss = 0, elf_brk = 0, start_code, end_code, end_data;
	int fd, ret, elf_type, elf_prot, load_addr_set = 0;
	char name[TASK_NAME_LEN], *elf_interpreter = NULL;
	struct elf_header elf_header, interp_elf_header;
	struct elf_prog_header *ph, *first_ph;
	struct dentry *interp_dentry = NULL;
	struct file *filp = NULL;
	void *buf_mmap;
	size_t size;

	/* check elf header */
	elf_header = *((struct elf_header *) bprm->buf);
	ret = elf_check(&elf_header);
	if (ret)
		return ret;

	/* open binary program */
	fd = open_dentry(bprm->dentry, O_RDONLY);
	if (fd < 0)
		return fd;

	/* get file */
	filp = current_task->files->filp[fd];

	/* save path */
	strncpy(name, bprm->filename, TASK_NAME_LEN - 1);
	name[TASK_NAME_LEN - 1] = 0;

	/* allocate program header */
	size = elf_header.e_phentsize * elf_header.e_phnum;
	first_ph = (struct elf_prog_header *) kmalloc(size);
	if (!first_ph) {
		ret = -ENOMEM;
		goto out;
	}

	/* read first program header */
	ret = read_exec(bprm->dentry, elf_header.e_phoff, (char *) first_ph, size);
	if (ret < 0)
		goto out;

	/* check if an ELF interpreter is needed */
	for (i = 0, ph = first_ph; i < elf_header.e_phnum; i++, ph++) {
		/* skip non interp segment */
		if (ph->p_type != PT_INTERP)
			continue;

		/* allocate interpreter path */
		elf_interpreter = (char *) kmalloc(ph->p_filesz);
		if (!elf_interpreter) {
			ret = -ENOMEM;
			goto out;
		}

		/* read interpreter path */
		ret = read_exec(bprm->dentry, ph->p_offset, elf_interpreter, ph->p_filesz);
		if (ret < 0)
			goto out;

		/* resolve interpreter path */
		interp_dentry = open_namei(AT_FDCWD, elf_interpreter, 1, 0);
		ret = PTR_ERR(interp_dentry);
		if (IS_ERR(interp_dentry))
			goto out;

		/* check permissions */
		ret = permission(interp_dentry->d_inode, MAY_EXEC);
		if (ret)
			goto out;

		/* read header */
		ret = read_exec(interp_dentry, 0, bprm->buf, 128);
		if (ret < 0)
			goto out;

		/* get exec headers */
		interp_elf_header = *((struct elf_header *) bprm->buf);
		break;
	}

	/* clear current executable */
	ret = clear_old_exec();
	if (ret)
		goto out;

	/* reset code */
	current_task->mm->end_data = 0;
	current_task->mm->end_code = 0;
	elf_entry = elf_header.e_entry;

	/* read each elf segment */
	for (i = 0, ph = first_ph; i < elf_header.e_phnum; i++, ph++) {
		/* skip non load segment */
		if (ph->p_type != PT_LOAD)
			continue;

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
		if (elf_header.e_type == ET_EXEC || load_addr_set)
			elf_type |= MAP_FIXED;
		else if (elf_header.e_type == ET_DYN)
			load_bias = ELF_PAGESTART(ELF_ET_DYN_BASE - ph->p_vaddr);

		/* map elf segment */
		buf_mmap = do_mmap(filp,
					ELF_PAGESTART(ph->p_vaddr + load_bias),
					ph->p_filesz + ELF_PAGEOFFSET(ph->p_vaddr),
					elf_prot,
					elf_type,
					ph->p_offset - ELF_PAGEOFFSET(ph->p_vaddr));
		if (!buf_mmap) {
			ret = -ENOMEM;
			goto out;
		}

		/* set load address */
		if (!load_addr_set) {
			load_addr_set = 1;
			load_addr = ph->p_vaddr - ph->p_offset;
			if (elf_header.e_type == ET_DYN) {
				load_bias += (uint32_t) buf_mmap - ELF_PAGESTART(load_bias + ph->p_vaddr);
				load_addr += load_bias;
			}
		}

		/* find the start of code section */
		k = ph->p_vaddr;
		if (k < start_code)
			start_code = k;

		/* find the end of the file mapping */
		k = ph->p_vaddr + ph->p_filesz;
		if (k > elf_bss)
			elf_bss = k;

		/* find the end of code section */
		if ((ph->p_flags & PF_X) && end_code < k)
			end_code = k;

		/* find the end of data section */
		if (end_data < k)
			end_data = k;

		/* find the end of the memory mapping */
		k = ph->p_vaddr + ph->p_memsz;
		if (k > elf_brk)
			elf_brk = k;
	}

	/* apply load bias */
	elf_entry += load_bias;
	elf_bss += load_bias;
	elf_brk += load_bias;
	start_code += load_bias;
	end_code += load_bias;
	end_data += load_bias;

	/* load ELF interpreter */
	if (elf_interpreter) {
		elf_entry = elf_load_interpreter(&interp_elf_header, interp_dentry, &interp_load_addr);
		if (elf_entry == ~0UL)
			goto out;
	}

	/* memzero fractionnal page of bss section */
	memset((void *) elf_bss, 0, PAGE_ALIGN_UP(elf_bss) - elf_bss);

	/* setup BSS and BRK sections */
	set_brk(elf_bss, elf_brk);

	/* setup stack */
	set_stack(bprm, &sp, &args_str);

	/* update task sections  */
	current_task->mm->start_brk = elf_brk;
	current_task->mm->end_brk = elf_brk + PAGE_SIZE;
	current_task->mm->start_code = start_code;
	current_task->mm->end_code = end_code;
	current_task->mm->end_data = end_data;

	/* create ELF tables */
	ret = elf_create_tables(bprm, (uint32_t *) sp, (char *) args_str, &elf_header, load_addr, load_bias, interp_load_addr);
	if (ret)
		goto out;

	/* change task name */
	memcpy(current_task->name, name, TASK_NAME_LEN);

	/* setup task entry and stack pointer */
	current_task->thread.regs.eip = elf_entry;
	current_task->thread.regs.useresp = sp;
out:
	sys_close(fd);
	dput(interp_dentry);
	kfree(elf_interpreter);
	kfree(first_ph);
	return ret;
}

/*
 * ELF binary format.
 */
static struct binfmt elf_format = {
	.load_binary		= elf_load_binary,
};

/*
 * Init ELF binary format.
 */
int init_elf_binfmt()
{
	return register_binfmt(&elf_format);
}