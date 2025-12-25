#include <x86/interrupt.h>
#include <x86/gdt.h>
#include <x86/ldt.h>
#include <mm/paging.h>
#include <proc/task.h>
#include <proc/sched.h>
#include <proc/elf.h>
#include <mm/paging.h>
#include <lib/semaphore.h>
#include <sys/syscall.h>
#include <stdio.h>
#include <string.h>
#include <stderr.h>
#include <fcntl.h>

/* switch to user mode (defined in x86/scheduler.s) */
extern void enter_user_mode(uint32_t esp, uint32_t eip, uint32_t return_address);
extern void return_user_mode(struct registers *regs);

/*
 * Kernel fork trampoline.
 */
static void task_user_entry(struct task *task)
{
	/* return to user mode */
	load_tss(task->thread.kernel_stack);
	return_user_mode(&task->thread.regs);
}

/*
 * Init (process 1) entry.
 */
static void init_entry(struct task *task)
{
	char *argv[] = { "init", NULL };
	int ret;

	/* open console */
	ret = sys_open("/dev/console", O_RDWR, 0);
	if (ret < 0)
		goto err;

	/* dup stdin, stdout, stderr */
	sys_dup(ret);
	sys_dup(ret);

	/* load init process */
	ret = sys_execve("/sbin/init", argv, NULL);
	if (ret)
		goto err;

	/* enter user mode */
	enter_user_mode(task->thread.regs.useresp, task->thread.regs.eip, TASK_RETURN_ADDRESS);
	return;
err:
	panic("cannot load init process");
}

/*
 * Copy flags.
 */
static int task_copy_flags(struct task *task, struct task *parent, uint32_t clone_flags)
{
	uint32_t new_flags = parent ? parent->flags : 0;

	/* clear flags */
	new_flags &= ~(CLONE_VFORK);

	/* set vfork */
	if (clone_flags & CLONE_VFORK)
		new_flags |= CLONE_VFORK;

	/* set new flags */
	task->flags = new_flags;

	/* don'y clone ptrace */
	if (!(clone_flags & CLONE_PTRACE))
		task->ptrace = 0;

	return 0;
}

/*
 * Copy signal handlers.
 */
static int task_copy_signals(struct task *task, struct task *parent, uint32_t clone_flags)
{
	/* clone files */
	if (clone_flags & CLONE_SIGHAND) {
		if (!parent)
			return -EINVAL;

		task->sig = parent->sig;
		task->sig->count++;
		return 0;
	}

	/* allocate signal structure */
	task->sig = (struct signal_struct *) kmalloc(sizeof(struct signal_struct));
	if (!task->sig)
		return -ENOMEM;

	/* init signal structure */
	memset(task->sig, 0, sizeof(struct signal_struct));
	task->sig->count = 1;

	/* init signals */
	sigemptyset(&task->blocked);
	sigemptyset(&task->saved_sigmask);
	init_sigpending(&task->pending);

	/* copy signals */
	if (parent)
		memcpy(task->sig->action, parent->sig->action, sizeof(task->sig->action));
	else
		memset(task->sig->action, 0, sizeof(task->sig->action));

	return 0;
}

/*
 * Copy resource limits.
 */
static int task_copy_rlim(struct task *task, struct task *parent)
{
	int i;

	/* copy parent limits */
	if (parent) {
		memcpy(task->rlim, parent->rlim, sizeof(parent->rlim));
		return 0;
	}

	/* default limits */
	for (i = 0; i < RLIM_NLIMITS; i++) {
		task->rlim[i].rlim_cur = LONG_MAX;
		task->rlim[i].rlim_max = LONG_MAX;
	}

	/* number of opened files and stack limit */
	task->rlim[RLIMIT_NOFILE].rlim_cur = NR_OPEN;
	task->rlim[RLIMIT_NOFILE].rlim_max = NR_OPEN;
	task->rlim[RLIMIT_STACK].rlim_cur = USTACK_LIMIT;
	task->rlim[RLIMIT_STACK].rlim_max = USTACK_LIMIT;

	return 0;
}

/*
 * Allocate a memory structure.
 */
struct mm_struct *task_alloc_mm()
{
	struct mm_struct *mm_new;

	/* allocate memory structure */
	mm_new = (struct mm_struct *) kmalloc(sizeof(struct mm_struct));
	if (!mm_new)
		return NULL;

	/* init memory structure */
	memset(mm_new, 0, sizeof(struct mm_struct));
	mm_new->count = 1;
	INIT_LIST_HEAD(&mm_new->vm_list);

	return mm_new;
}

/*
 * Duplicate a memory structure.
 */
struct mm_struct *task_dup_mm(struct mm_struct *mm)
{
	struct vm_area *vma_parent, *vma_child;
	struct mm_struct *mm_new;
	struct list_head *pos;
	int ret;

	/* allocate memory structure */
	mm_new = task_alloc_mm();
	if (!mm_new)
		return NULL;

	/* first task = use kernel pgd */
	if (!mm) {
		mm_new->pgd = pgd_kernel;
		return mm_new;
	}

	/* clone page directory */
	mm_new->pgd = create_page_directory();
	if (!mm_new->pgd)
		goto err;

	/* clone LDT */
	if (mm->ldt_size) {
		ret = clone_ldt(mm, mm_new);
		if (ret)
			goto err;
	}

	/* copy code/data/brk start/end */
	mm_new->start_code = mm->start_code;
	mm_new->end_code = mm->end_code;
	mm_new->start_data = mm->start_data;
	mm_new->end_data = mm->end_data;
	mm_new->start_brk = mm->start_brk;
	mm_new->end_brk = mm->end_brk;
	mm_new->arg_start = mm->arg_end;
	mm_new->arg_start = mm->arg_end;
	mm_new->env_start = mm->env_start;
	mm_new->env_end = mm->env_end;

	/* copy virtual memory areas */
	list_for_each(pos, &mm->vm_list) {
		vma_parent = list_entry(pos, struct vm_area, list);
		vma_child = (struct vm_area *) kmalloc(sizeof(struct vm_area));
		if (!vma_child)
			goto err;

		/* copy memory area */
		memset(vma_child, 0, sizeof(struct vm_area));
		vma_child->vm_start = vma_parent->vm_start;
		vma_child->vm_end = vma_parent->vm_end;
		vma_child->vm_flags = vma_parent->vm_flags;
		vma_child->vm_page_prot = vma_parent->vm_page_prot;
		vma_child->vm_offset = vma_parent->vm_offset;
		vma_child->vm_file = vma_parent->vm_file;
		if (vma_child->vm_file)
			vma_child->vm_file->f_count++;
		vma_child->vm_ops = vma_parent->vm_ops;
		vma_child->vm_mm = mm_new;
		list_add_tail(&vma_child->list, &mm_new->vm_list);

		/* copy pages */
		copy_page_range(mm->pgd, mm_new->pgd, vma_child);

		/* open region */
		if (vma_child->vm_ops && vma_child->vm_ops->open)
			vma_child->vm_ops->open(vma_child);
	}

	/* flush tlb */
	flush_tlb(current_task->mm->pgd);

	return mm_new;
err:
	if (mm_new->pgd && mm_new->pgd != pgd_kernel)
		free_pgd(mm_new->pgd);
	task_exit_mmap(mm_new);
	kfree(mm_new);
	return NULL;
}

/*
 * Copy memory areas.
 */
static int task_copy_mm(struct task *task, struct task *parent, uint32_t clone_flags)
{
	/* clone mm struct */
	if (clone_flags & CLONE_VM) {
		if (!parent)
			return -EINVAL;

		task->mm = parent->mm;
		task->mm->count++;
		return 0;
	}

	/* duplicate mm struct */
	task->mm = task_dup_mm(parent ? parent->mm : NULL);
	if (!task->mm)
		return -ENOMEM;

	return 0;
}

/*
 * Copy file system informations.
 */
static int task_copy_fs(struct task *task, struct task *parent, uint32_t clone_flags)
{
	/* clone fs */
	if (clone_flags & CLONE_FS) {
		if (!parent)
			return -EINVAL;

		task->fs = parent->fs;
		task->fs->count++;
		return 0;
	}

	/* allocate file system structure */
	task->fs = (struct fs_struct *) kmalloc(sizeof(struct fs_struct));
	if (!task->fs)
		return -ENOMEM;

	/* init file system structure */
	memset(task->fs, 0, sizeof(struct fs_struct));
	task->fs->count = 1;
	task->fs->pwd = NULL;
	task->fs->root = NULL;

	/* set umask */
	task->fs->umask = parent ? parent->fs->umask : 0022;

	/* duplicate current working dir */
	if (parent && parent->fs->pwd)
		task->fs->pwd = dget(parent->fs->pwd);

	/* duplicate root dir */
	if (parent && parent->fs->root)
		task->fs->root = dget(parent->fs->root);

	return 0;
}

/*
 * Copy files.
 */
static int task_copy_files(struct task *task, struct task *parent, uint32_t clone_flags)
{
	int i;

	/* clone files */
	if (clone_flags & CLONE_FILES) {
		if (!parent)
			return -EINVAL;

		task->files = parent->files;
		task->fs->count++;
		return 0;
	}

	/* allocate file structure */
	task->files = (struct files_struct *) kmalloc(sizeof(struct files_struct));
	if (!task->files)
		return -ENOMEM;

	/* init files structure */
	memset(task->files, 0, sizeof(struct files_struct));
	task->files->count = 1;

	/* copy close on exec bitmap */
	if (parent)
		memcpy(&task->files->close_on_exec, &parent->files->close_on_exec, sizeof(fd_set_t));

	/* copy open files */
	for (i = 0; i < NR_OPEN; i++) {
		task->files->filp[i] = parent ? parent->files->filp[i] : NULL;
		if (task->files->filp[i])
			task->files->filp[i]->f_count++;
	}

	return 0;
}

/*
 * Copy thread.
 */
static int task_copy_thread(struct task *task, struct task *parent, uint32_t user_sp)
{
	int ret = 0;

	if (parent) {
		/* duplicate user registers */
		memcpy(&task->thread.regs, &parent->thread.regs, sizeof(struct registers));
		task->thread.regs.eax = 0;

		/* duplicate TLS */
		memcpy(&task->thread.tls, &parent->thread.tls, sizeof(struct desc_struct));
	} else {
		gdt_read_entry(GDT_ENTRY_TLS, &task->thread.tls);
	}

	/* set user stack */
	if (user_sp)
		task->thread.regs.useresp = user_sp;

	return ret;
}

/*
 * Exit signals.
 */
void task_exit_signals(struct task *task)
{
	struct signal_struct *sig = task->sig;

	/* flush signals */
	flush_signals(task);

	/* free signals */
	if (sig) {
		task->sig = NULL;

		if (--sig->count <= 0)
			kfree(sig);
	}
}

/*
 * Exit file system.
 */
void task_exit_fs(struct task *task)
{
	struct fs_struct *fs = task->fs;

	if (fs) {
		task->fs = NULL;

		if (--fs->count <= 0) {
			dput(fs->root);
			dput(fs->pwd);
			kfree(fs);
		}
	}
}

/*
 * Exit opened files.
 */
void task_exit_files(struct task *task)
{
	struct files_struct *files = task->files;
	struct file *filp;
	int i;

	if (files) {
		task->files = NULL;

		if (--files->count <= 0) {
			for (i = 0; i < NR_OPEN; i++) {
				filp = files->filp[i];

				if (filp) {
					files->filp[i] = NULL;
					close_fp(filp);
				}
			}

			kfree(files);
		}
	}
}

/*
 * Exit mmap.
 */
void task_exit_mmap(struct mm_struct *mm)
{
	struct list_head *pos, *n;
	struct vm_area *vma;

	/* clear memory size */
	mm->rss = 0;

	/* free memory regions */
	list_for_each_safe(pos, n, &mm->vm_list) {
		vma = list_entry(pos, struct vm_area, list);
		if (vma) {
			/* unmap and close */
			if (vma->vm_ops && vma->vm_ops->unmap)
				vma->vm_ops->unmap(vma, vma->vm_start, vma->vm_end - vma->vm_start);
			if (vma->vm_ops && vma->vm_ops->close)
				vma->vm_ops->close(vma);

			/* unmap pages */
			zap_page_range(mm->pgd, vma->vm_start, vma->vm_end - vma->vm_start);

			/* release file */
			if (vma->vm_file)
				fput(vma->vm_file);

			/* free memory region */
			remove_vma(vma);
			kfree(vma);
		}
	}
}

/*
 * Exit memory.
 */
void task_exit_mm(struct task *task)
{
	struct mm_struct *mm = task->mm;

	if (mm) {
		task_release_mmap(task);

		if (--mm->count <= 0) {
			/* free areas */
			task_exit_mmap(mm);

			/* free page directory */
			if (mm->pgd && mm->pgd != pgd_kernel)
				free_pgd(mm->pgd);

			/* free LDT */
			if (mm->ldt)
				kfree(mm->ldt);

			kfree(mm);
		}

		task->mm = NULL;
	}
}

/*
 * Release mmap : wake up parent sleeping on vfork semaphore.
 */
void task_release_mmap(struct task *task)
{
	if (task->flags & CLONE_VFORK)
		up(task->parent->vfork_sem);
}

/*
 * Create and init a task.
 */
static struct task *create_task(struct task *parent, uint32_t clone_flags, uint32_t user_sp)
{
	struct task *task;
	void *stack;

	/* create task */
	task = (struct task *) kmalloc(sizeof(struct task));
	if (!task)
		return NULL;

	/* reset task */
	memset(task, 0, sizeof(struct task));

	/* allocate stack */
	stack = (void *) kmalloc(STACK_SIZE);
	if (!stack)
		goto err_stack;

	/* set stack */
	memset(stack, 0, STACK_SIZE);
	task->thread.kernel_stack = (uint32_t) stack + STACK_SIZE;
	task->thread.esp = task->thread.kernel_stack - sizeof(struct task_registers);

	/* init task */
	task->pid = get_next_pid();
	task->pgrp = parent ? parent->pgrp : task->pid;
	task->session = parent ? parent->session : task->pid;
	task->state = TASK_RUNNING;
	task->counter = parent ? (parent->counter >>= 1) : DEF_PRIORITY;
	task->priority = parent ? parent->priority : DEF_PRIORITY;
	task->parent = parent;
	task->uid = parent ? parent->uid : 0;
	task->euid = parent ? parent->euid : 0;
	task->suid = parent ? parent->suid : 0;
	task->fsuid = parent ? parent->fsuid : 0;
	task->gid = parent ? parent->gid : 0;
	task->egid = parent ? parent->egid : 0;
	task->sgid = parent ? parent->sgid : 0;
	task->fsgid = parent ? parent->fsgid : 0;
	task->tty = parent ? parent->tty : 0;
	task->ptrace = parent ? parent->ptrace : 0;
	task->start_time = jiffies;
	task->vfork_sem = NULL;
	INIT_LIST_HEAD(&task->list);
	INIT_LIST_HEAD(&task->real_timer.list);

	/* copy task name and TLS */
	if (parent)
		memcpy(task->name, parent->name, TASK_NAME_LEN);

	/* copy task */
	if (task_copy_flags(task, parent, clone_flags))
		goto err_flags;
	if (task_copy_mm(task, parent, clone_flags))
		goto err_mm;
	if (task_copy_fs(task, parent, clone_flags))
		goto err_fs;
	if (task_copy_files(task, parent, clone_flags))
		goto err_files;
	if (task_copy_signals(task, parent, clone_flags))
		goto err_signals;
	if (task_copy_rlim(task, parent))
		goto err_rlim;
	if (task_copy_thread(task, parent, user_sp))
		goto err_thread;

	/* update number of tasks */
	nr_tasks++;

	return task;
err_thread:
	task_exit_signals(task);
err_rlim:
err_signals:
	task_exit_files(task);
err_files:
	task_exit_fs(task);
err_fs:
	task_exit_mm(task);
err_mm:
err_flags:
	kfree(stack);
err_stack:
	kfree(task);
	return NULL;
}

/*
 * Create a kernel thread.
 */
int kernel_thread(int (*func)(void *), void *arg, uint32_t flags)
{
	long ret, d0;

	__asm__ __volatile__(
		"movl %%esp,%%esi		\n\t"
		"int $0x80			\n\t"		/* Linux/i386 system call */
		"cmpl %%esp,%%esi		\n\t"		/* child or parent? */
		"je 1f				\n\t"		/* parent - jump */
		"movl %4,%%eax			\n\t"
		"pushl %%eax			\n\t"
		"call *%5			\n\t"		/* call func */
		"movl %3,%0			\n\t"		/* exit */
		"int $0x80			\n"
		"1:				\t"
		:"=&a" (ret), "=&S" (d0)
		:"0" (__NR_clone), "i" (__NR_exit),
		 "r" (arg), "r" (func),
		 "b" (flags | CLONE_VM)
		: "memory");

	return ret;
}

/*
 * Fork a task.
 */
int do_fork(uint32_t clone_flags, uint32_t user_sp)
{
	struct task_registers *regs;
	struct semaphore sem;
	struct task *task;

	/* create task */
	task = create_task(current_task, clone_flags, user_sp);
	if (!task)
		return -EINVAL;

	/* set registers */
	regs = (struct task_registers *) task->thread.esp;
	memset(regs, 0, sizeof(struct task_registers));

	/* set eip to function */
	regs->parameter1 = (uint32_t) task;
	regs->return_address = TASK_RETURN_ADDRESS;
	regs->eip = (uint32_t) task_user_entry;

	/* add new task */
	list_add(&task->list, &tasks_list);

	/* vfork : sleep on semaphore */
	if (clone_flags & CLONE_VFORK) {
		init_semaphore(&sem, 0);
		current_task->vfork_sem = &sem;
		down(&sem);
		current_task->vfork_sem = NULL;
	}

	return task->pid;
}

/*
 * Create kinit (kernel init + idle) process.
 */
struct task *create_kinit_task(void (*kinit_func)())
{
	struct task_registers *regs;
	struct task *task;

	/* create task */
	task = create_task(NULL, 0, 0);
	if (!task)
		return NULL;

	/* set registers */
	regs = (struct task_registers *) task->thread.esp;
	memset(regs, 0, sizeof(struct task_registers));

	/* set eip to function */
	regs->parameter1 = (uint32_t) NULL;
	regs->return_address = TASK_RETURN_ADDRESS;
	regs->eip = (uint32_t) kinit_func;

	return task;
}

/*
 * Create init process.
 */
struct task *create_init_task(struct task *parent)
{
	struct task_registers *regs;
	struct task *task;

	/* create task */
	task = create_task(parent, 0, 0);
	if (!task)
		return NULL;

	/* set registers */
	regs = (struct task_registers *) task->thread.esp;
	memset(regs, 0, sizeof(struct task_registers));

	/* set eip */
	regs->parameter1 = (uint32_t) task;
	regs->return_address = TASK_RETURN_ADDRESS;
	regs->eip = (uint32_t) init_entry;

	/* add task */
	list_add(&task->list, &tasks_list);

	return task;
}

/*
 * Destroy a task.
 */
void destroy_task(struct task *task)
{
	if (!task)
		return;

	/* remove task */
	list_del(&task->list);

	/* free kernel stack */
	kfree((void *) (task->thread.kernel_stack - STACK_SIZE));

	/* free task */
	kfree(task);

	/* update number of tasks */
	nr_tasks--;
}

/*
 * Is a task in a group ?
 */
int task_in_group(struct task *task, gid_t gid)
{
	size_t i;

	if (task->fsgid == gid)
		return 1;

	for (i = 0; i < task->ngroups; i++)
		if (current_task->groups[i] == gid)
			return 1;

	return 0;
}

/*
 * Fork system call.
 */
pid_t sys_fork()
{
	return do_fork(0, 0);
}

/*
 * Vfork system call..
 */
pid_t sys_vfork()
{
	return do_fork(CLONE_VFORK | CLONE_VM, 0);
}

/*
 * Clone system call.
 */
int sys_clone(uint32_t flags, uint32_t newsp)
{
	return do_fork(flags, newsp);
}
