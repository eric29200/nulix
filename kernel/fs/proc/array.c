#include <fs/proc_fs.h>
#include <proc/sched.h>
#include <drivers/char/tty.h>
#include <stdio.h>
#include <dev.h>

/*
 * Process states.
 */
static char proc_states[] = {
	'R',				/* running */
	'S',				/* sleeping */
	'T',				/* stopped */
	'Z',				/* zombie */
};

/*
 * Process states.
 */
static char *proc_states_desc[] = {
	"R (running)",			/* running */
	"S (sleeping)",			/* sleeping */
	"T (stopped)",			/* stopped */
	"Z' (zombie)"			/* zombie */
};

/*
 * Read process stat.
 */
int proc_stat_read(struct task *task, char *page)
{
	struct list_head *pos;
	struct vm_area *vma;
	size_t vsize = 0;

	/* compute virtual memory */
	list_for_each(pos, &task->mm->vm_list) {
		vma = list_entry(pos, struct vm_area, list);
		vsize += vma->vm_end - vma->vm_start;
	}

	/* print informations in temporary buffer */
	return sprintf(page,	"%d (%s) "						/* pid, name */
				"%c %d "						/* state, ppid */
				"%d %d "						/* pgrp, session */
				"%d "							/* tty */
				"0 0 "							/* tpgid, flags */
				"0 0 0 0 "						/* minflt, cminflt, majflt, cmajflt */
				"%ld %ld %ld %ld "					/* utime, stime, cutime, cstime */
				"0 0 "							/* priority, nice */
				"0 0 "							/* num_threads, itrealvalue */
				"%ld "							/* starttime */
				"%d %d 0 \n",						/* vsize, rss, rsslim */
				task->pid, task->name,
				proc_states[task->state - 1], task->parent ? task->parent->pid : task->pid,
				task->pgrp, task->session,
				task->tty ? dev_t_to_nr(task->tty->device) : 0,
				task->utime, task->stime, task->cutime, task->cstime,
				task->start_time,
				vsize, task->mm->rss
			);
}

/*
 * Get task name.
 */
static char *task_name(struct task *task, char *buf)
{
	return buf + sprintf(buf, 	"Name:\t%s\n", task->name);
}

/*
 * Get task state.
 */
static char *task_state(struct task *task, char *buf)
{
	return buf + sprintf(buf, 	"State:\t%s\n"
					"Tgid:\t0\n"
					"Ngid:\t0\n"
					"Pid:\t%d\n"
					"PPid:\t%d\n"
					"TracerPid:\t0\n"
					"Uid:\t%d\t%d\t%d\t%d\n"
					"Gid:\t%d\t%d\t%d\t%d\n"
					"FDSize:\t0\n"
					"Groups:\t0\n",
					proc_states_desc[task->state - 1],
					task->pid,
					task->parent ? task->parent->pid : task->pid,
					task->uid, task->euid, task->suid, task->fsuid,
					task->gid, task->egid, task->sgid, task->fsgid
				);
}

/*
 * Get task memory.
 */
static char *task_mem(struct task *task, char *buf)
{
	size_t len, vsize = 0, data = 0, stack = 0, exec = 0, lib = 0;
	struct list_head *pos;
	struct vm_area *vma;

	/* compute virtual memory */
	list_for_each(pos, &task->mm->vm_list) {
		vma = list_entry(pos, struct vm_area, list);
		len = (vma->vm_end - vma->vm_start) >> 10;

		/* update virtual size */
		vsize += len;

		/* update data/stack size */
		if (!vma->vm_file) {
			if (vma->vm_flags & VM_GROWSDOWN)
				stack += len;
			else
				data += len;

			continue;
		}

		/* update exec/lib size */
		if (!(vma->vm_flags & VM_WRITE) && vma->vm_flags & VM_EXEC) {
			if (vma->vm_flags & VM_EXECUTABLE)
				exec += len;
			else
				lib += len;
		}
	}

	return buf + sprintf(buf, 	"VmSize:\t%d kB\n"
					"VmLck:\t0 kB\n"
					"VmRSS:\t%d kB\n"
					"VmData:\t%d kB\n"
					"VmStk:\t%d kB\n"
					"VmExe:\t%d kB\n"
					"VmLib:\t%d kB\n",
					vsize,
					task->mm->rss << (PAGE_SHIFT - 10),
					data,
					stack,
					exec,
					lib
				);
}

/*
 * Read process status.
 */
int proc_status_read(struct task *task, char *page)
{
	char *ori = page;

	page = task_name(task, page);
	page = task_state(task, page);
	page = task_mem(task, page);

	return page - ori;
}

/*
 * Statistics on pages.
 */
static void statm_pte_range(pmd_t *pmd, uint32_t address, size_t size, size_t *pages, size_t *shared, size_t *total)
{
	struct page *page;
	pte_t *ptep, pte;
	uint32_t end;

	if (pmd_none(*pmd))
		return;

	/* get first entry */
	ptep = pte_offset(pmd, address);

	/* compute end address */
	address &= ~PMD_MASK;
	end = address + size;
	if (end > PMD_SIZE)
		end = PMD_SIZE;

	/* for each page table entry */
	do {
		pte = *ptep;

		/* go to next entry */
		address += PAGE_SIZE;
		ptep++;

		if (pte_none(pte))
			continue;

		/* update total */
		*total +=1;

		if (!pte_present(pte))
			continue;

		/* virtual page */
		page = pte_page(pte);
		if (!VALID_PAGE(page))
			continue;

		/* update number of present pages */
		*pages += 1;

		/* update number of shared pages */
		if (page->count > 1)
			*shared += 1;
	} while (address < end);
}

/*
 * Statistics on pages.
 */
static void statm_pmd_range(pgd_t *pgd, uint32_t address, size_t size, size_t *pages, size_t *shared, size_t *total)
{
	uint32_t end;
	pmd_t *pmd;

	/* get page table */
	pmd = pmd_offset(pgd);

	/* compute end address */
	address &= ~PGDIR_MASK;
	end = address + size;
	if (end > PGDIR_SIZE)
		end = PGDIR_SIZE;

	/* for each page table */
	do {
		statm_pte_range(pmd, address, end - address, pages, shared, total);
		address = (address + PMD_SIZE) & PMD_MASK;
		pmd++;
	} while (address < end);
}

/*
 * Statistics on pages.
 */
static void statm_pgd_range(pgd_t *pgd, uint32_t address, uint32_t end, size_t *pages, size_t *shared, size_t *total)
{
	/* for each page directory */
	while (address < end) {
		statm_pmd_range(pgd, address, end - address, pages, shared, total);
		address = (address + PGDIR_SIZE) & PGDIR_MASK;
		pgd++;
	}
}

/*
 * Read process memory.
 */
int proc_statm_read(struct task *task, char *page)
{
	size_t size = 0, resident = 0, share = 0, text = 0;
	struct list_head *pos;
	struct vm_area *vma;
	pgd_t *pgd;

	/* compute virtual memory */
	list_for_each(pos, &task->mm->vm_list) {
		vma = list_entry(pos, struct vm_area, list);

		/* stat pages */
		size_t pages = 0, shared = 0, total = 0;
		pgd = pgd_offset(current_task->mm->pgd, vma->vm_start);
		statm_pgd_range(pgd, vma->vm_start, vma->vm_end, &pages, &shared, &total);

		/* update total, resident, shared and dirty pages */
		size += total;
		resident += pages;
		share += shared;

		/* update text, data, stack and library pages */
		if (vma->vm_flags & VM_EXECUTABLE)
			text += pages;
	}

	return sprintf(page, "%d %d %d %d 0 %d 0\n", size, resident, share, text, size - share);
}

/*
 * Read process command line.
 */
int proc_cmdline_read(struct task *task, char *page)
{
	char *p, *arg_str;
	uint32_t arg;

	/* switch to task's pgd */
	switch_pgd(task->mm->pgd);

	/* get arguments */
	for (arg = task->mm->arg_start, p = page; arg != task->mm->arg_end; arg += sizeof(char *)) {
		arg_str = *((char **) arg);

		/* copy argument */
		while (*arg_str && p - page < PAGE_SIZE)
			*p++ = *arg_str++;

		/* overflow */
		if (p - page >= PAGE_SIZE)
			break;

		/* end argument */
		*p++ = 0;
	}

	/* switch back to current's pgd */
	switch_pgd(current_task->mm->pgd);

	return p - page;
}

/*
 * Read process environ.
 */
int proc_environ_read(struct task *task, char *page)
{
	char *p, *environ_str;
	uint32_t environ;

	/* switch to task's pgd */
	switch_pgd(task->mm->pgd);

	/* get environs */
	for (environ = task->mm->env_start, p = page; environ != task->mm->env_end; environ += sizeof(char *)) {
		environ_str = *((char **) environ);

		/* copy environ */
		while (*environ_str && p - page < PAGE_SIZE)
			*p++ = *environ_str++;

		/* overflow */
		if (p - page >= PAGE_SIZE)
			break;

		/* end environ */
		*p++ = 0;
	}

	/* switch back to current's pgd */
	switch_pgd(current_task->mm->pgd);

	return p - page;
}

/*
 * Read process io.
 */
int proc_io_read(struct task *task, char *page)
{
	struct task_io_accounting acct = task->ioac;

	return sprintf(page,
			"rchar: %llu\n"
			"wchar: %llu\n"
			"syscr: %llu\n"
			"syscw: %llu\n"
			"read_bytes: %llu\n"
			"write_bytes: %llu\n"
			"cancelled_write_bytes: %llu\n",
			acct.rchar,
			acct.wchar,
			acct.syscr,
			acct.syscw,
			acct.read_bytes,
			acct.write_bytes,
			acct.cancelled_write_bytes);
}