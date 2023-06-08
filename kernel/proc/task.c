#include <x86/interrupt.h>
#include <x86/tss.h>
#include <mm/mm.h>
#include <proc/task.h>
#include <proc/sched.h>
#include <proc/elf.h>
#include <ipc/semaphore.h>
#include <sys/syscall.h>
#include <stdio.h>
#include <string.h>
#include <stderr.h>
#include <fcntl.h>

/* switch to user mode (defined in x86/scheduler.s) */
extern void enter_user_mode(uint32_t esp, uint32_t eip, uint32_t return_address);
extern void return_user_mode(struct registers_t *regs);

/*
 * Kernel fork trampoline.
 */
static void task_user_entry(struct task_t *task)
{
	/* return to user mode */
	tss_set_stack(0x10, task->kernel_stack);
	return_user_mode(&task->user_regs);
}

/*
 * Init (process 1) entry.
 */
static void init_entry(struct task_t *task)
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
	enter_user_mode(task->user_regs.useresp, task->user_regs.eip, TASK_RETURN_ADDRESS);
	return;
err:
	panic("cannot load init process");
}

/*
 * Copy flags.
 */
static int task_copy_flags(struct task_t *task, struct task_t *parent, uint32_t clone_flags)
{
	uint32_t new_flags = parent ? parent->flags : 0;

	/* clear flags */
	new_flags &= ~(CLONE_VFORK);

	/* set vfork */
	if (clone_flags & CLONE_VFORK)
		new_flags |= CLONE_VFORK;

	/* set new flags */
	task->flags = new_flags;

	return 0;
}

/*
 * Copy signal handlers.
 */
static int task_copy_signals(struct task_t *task, struct task_t *parent, uint32_t clone_flags)
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
	sigemptyset(&task->sigpend);
	sigemptyset(&task->sigmask);
	sigemptyset(&task->saved_sigmask);

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
static int task_copy_rlim(struct task_t *task, struct task_t *parent)
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
 * Duplicate a memory structure.
 */
struct mm_struct *task_dup_mm(struct mm_struct *mm)
{
	struct vm_area_t *vm_parent, *vm_child;
	struct mm_struct *mm_new;
	struct list_head_t *pos;

	/* allocate memory structure */
	mm_new = (struct mm_struct *) kmalloc(sizeof(struct mm_struct));
	if (!mm_new)
		return NULL;

	/* init memory structure */
	memset(mm_new, 0, sizeof(struct mm_struct));
	mm_new->count = 1;
	INIT_LIST_HEAD(&mm_new->vm_list);

	/* clone page directory */
	mm_new->pgd = mm ? clone_page_directory(mm->pgd) : kernel_pgd;
	if (!mm_new->pgd)
		goto err;

	/* copy text/brk start/end */
	mm_new->start_text = mm ? mm->start_text : 0;
	mm_new->end_text = mm ? mm->end_text : 0;
	mm_new->start_brk = mm ? mm->start_brk : 0;
	mm_new->end_brk = mm ? mm->end_brk : 0;

	/* copy virtual memory areas */
	if (mm) {
		list_for_each(pos, &mm->vm_list) {
			vm_parent = list_entry(pos, struct vm_area_t, list);
			vm_child = (struct vm_area_t *) kmalloc(sizeof(struct vm_area_t));
			if (!vm_child)
				goto err;

			/* copy memory area */
			memset(vm_child, 0, sizeof(struct vm_area_t));
			vm_child->vm_start = vm_parent->vm_start;
			vm_child->vm_end = vm_parent->vm_end;
			vm_child->vm_flags = vm_parent->vm_flags;
			vm_child->vm_page_prot = vm_parent->vm_page_prot;
			vm_child->vm_offset = vm_parent->vm_offset;
			vm_child->vm_inode = vm_parent->vm_inode;
			vm_child->vm_ops = vm_parent->vm_ops;
			list_add_tail(&vm_child->list, &mm_new->vm_list);

			/* open region */
			if (vm_child->vm_ops && vm_child->vm_ops->open)
				vm_child->vm_ops->open(vm_child);

			/* shared memory : unmap pages = force page fault, to read from page cache */
			if (vm_child->vm_flags & VM_SHARED)
				unmap_pages(vm_child->vm_start, vm_child->vm_end, mm_new->pgd);
		}
	}

	return mm_new;
err:
	if (mm_new->pgd && mm_new->pgd != kernel_pgd)
		free_page_directory(mm_new->pgd);
	task_exit_mmap(mm_new);
	kfree(mm_new);
	return NULL;
}
/*
 * Copy memory areas.
 */
static int task_copy_mm(struct task_t *task, struct task_t *parent, uint32_t clone_flags)
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
static int task_copy_fs(struct task_t *task, struct task_t *parent, uint32_t clone_flags)
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
	task->fs->cwd = NULL;
	task->fs->root = NULL;

	/* set umask */
	task->fs->umask = parent ? parent->fs->umask : 0022;

	/* duplicate current working dir */
	if (parent && parent->fs->cwd) {
		task->fs->cwd = parent->fs->cwd;
		task->fs->cwd->i_ref++;
	}

	/* duplicate root dir */
	if (parent && parent->fs->root) {
		task->fs->root = parent->fs->root;
		task->fs->root->i_ref++;
	}

	return 0;
}

/*
 * Copy files.
 */
static int task_copy_files(struct task_t *task, struct task_t *parent, uint32_t clone_flags)
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
			task->files->filp[i]->f_ref++;
	}

	return 0;
}

/*
 * Copy thread.
 */
static int task_copy_thread(struct task_t *task, struct task_t *parent, uint32_t user_sp)
{
	/* duplicate parent registers */
	if (parent) {
		memcpy(&task->user_regs, &parent->user_regs, sizeof(struct registers_t));
		task->user_regs.eax = 0;
	}

	/* set user stack */
	if (user_sp)
		task->user_regs.useresp = user_sp;

	return 0;
}

/*
 * Exit signals.
 */
void task_exit_signals(struct task_t *task)
{
	struct signal_struct *sig = task->sig;

	if (sig) {
		task->sig = NULL;

		if (--sig->count <= 0)
			kfree(sig);
	}
}

/*
 * Exit file system.
 */
void task_exit_fs(struct task_t *task)
{
	struct fs_struct *fs = task->fs;

	if (fs) {
		task->fs = NULL;

		if (--fs->count <= 0) {
			iput(fs->root);
			iput(fs->cwd);
			kfree(fs);
		}
	}
}

/*
 * Exit opened files.
 */
void task_exit_files(struct task_t *task)
{
	struct files_struct *files = task->files;
	int i;

	if (files) {
		task->files = NULL;

		if (--files->count <= 0) {
			for (i = 0; i < NR_OPEN; i++)
				if (files->filp[i])
					do_close(files->filp[i]);

			kfree(files);
		}
	}
}

/*
 * Exit mmap.
 */
void task_exit_mmap(struct mm_struct *mm)
{
	struct list_head_t *pos, *n;
	struct vm_area_t *vm_area;

	/* free memory regions */
	list_for_each_safe(pos, n, &mm->vm_list) {
		vm_area = list_entry(pos, struct vm_area_t, list);
		if (vm_area) {
			/* release inode */
			if (vm_area->vm_ops && vm_area->vm_ops->close)
				vm_area->vm_ops->close(vm_area);

			/* unmap pages */
			unmap_pages(vm_area->vm_start, vm_area->vm_end, mm->pgd);

			/* free memory region */
			list_del(&vm_area->list);
			kfree(vm_area);
		}
	}
}

/*
 * Exit memory.
 */
static void task_exit_mm(struct task_t *task)
{
	struct mm_struct *mm = task->mm;

	if (mm) {
		task->mm = NULL;

		if (--mm->count <= 0) {
			task_exit_mmap(mm);

			/* free page directory */
			if (mm->pgd && mm->pgd != kernel_pgd)
				free_page_directory(mm->pgd);

			kfree(mm);
		}
	}
}

/*
 * Release mmap : wake up parent sleeping on vfork semaphore.
 */
void task_release_mmap(struct task_t *task)
{
	if (task->flags & CLONE_VFORK)
		up(task->parent->vfork_sem);
}

/*
 * Create and init a task.
 */
static struct task_t *create_task(struct task_t *parent, uint32_t clone_flags, uint32_t user_sp, int kernel_thread)
{
	struct task_t *task;
	void *stack;

	/* create task */
	task = (struct task_t *) kmalloc(sizeof(struct task_t));
	if (!task)
		return NULL;

	/* reset task */
	memset(task, 0, sizeof(struct task_t));

	/* allocate stack */
	stack = (void *) kmalloc(STACK_SIZE);
	if (!stack)
		goto err_stack;

	/* set stack */
	memset(stack, 0, STACK_SIZE);
	task->kernel_stack = (uint32_t) stack + STACK_SIZE;
	task->esp = task->kernel_stack - sizeof(struct task_registers_t);

	/* init task */
	task->pid = kernel_thread ? 0 : get_next_pid();
	task->pgrp = parent ? parent->pgrp : task->pid;
	task->session = parent ? parent->session : task->pid;
	task->state = TASK_RUNNING;
	task->parent = parent;
	task->uid = parent ? parent->uid : 0;
	task->euid = parent ? parent->euid : 0;
	task->suid = parent ? parent->euid : 0;
	task->gid = parent ? parent->gid : 0;
	task->egid = parent ? parent->egid : 0;
	task->sgid = parent ? parent->sgid : 0;
	task->tty = parent ? parent->tty : 0;
	task->start_time = jiffies;
	task->vfork_sem = NULL;
	INIT_LIST_HEAD(&task->list);
	INIT_LIST_HEAD(&task->sig_tm.list);

	/* copy task name and TLS */
	if (parent) {
		memcpy(task->name, parent->name, TASK_NAME_LEN);
		memcpy(&task->tls, &parent->tls, sizeof(struct user_desc_t));
	} else {
		memset(task->name, 0, TASK_NAME_LEN);
		memset(&task->tls, 0, sizeof(struct user_desc_t));
	}

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
 * Create kernel thread.
 */
struct task_t *create_kernel_thread(void (*func)(void *), void *arg)
{
	struct task_registers_t *regs;
	struct task_t *task;

	/* create task */
	task = create_task(NULL, 0, 0, 1);
	if (!task)
		return NULL;

	/* set registers */
	regs = (struct task_registers_t *) task->esp;
	memset(regs, 0, sizeof(struct task_registers_t));

	/* set eip to function */
	regs->parameter1 = (uint32_t) arg;
	regs->return_address = TASK_RETURN_ADDRESS;
	regs->eip = (uint32_t) func;

	/* add task */
	list_add(&task->list, &tasks_list);

	return task;
}

/*
 * Fork a task.
 */
int do_fork(uint32_t clone_flags, uint32_t user_sp)
{
	struct task_registers_t *regs;
	struct semaphore_t sem;
	struct task_t *task;

	/* create task */
	task = create_task(current_task, clone_flags, user_sp, 0);
	if (!task)
		return -EINVAL;

	/* set registers */
	regs = (struct task_registers_t *) task->esp;
	memset(regs, 0, sizeof(struct task_registers_t));

	/* set eip to function */
	regs->parameter1 = (uint32_t) task;
	regs->return_address = TASK_RETURN_ADDRESS;
	regs->eip = (uint32_t) task_user_entry;

	/* add new task */
	list_add(&task->list, &current_task->list);

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
 * Create init process.
 */
struct task_t *create_init_task(struct task_t *parent)
{
	struct task_registers_t *regs;
	struct task_t *task;

	/* create task */
	task = create_task(parent, 0, 0, 0);
	if (!task)
		return NULL;

	/* set registers */
	regs = (struct task_registers_t *) task->esp;
	memset(regs, 0, sizeof(struct task_registers_t));

	/* set eip */
	regs->parameter1 = (uint32_t) task;
	regs->return_address = TASK_RETURN_ADDRESS;
	regs->eip = (uint32_t) init_entry;

	/* add task */
	list_add(&task->list, &current_task->list);

	return task;
}

/*
 * Destroy a task.
 */
void destroy_task(struct task_t *task)
{
	if (!task)
		return;

	/* remove task */
	list_del(&task->list);

	/* free kernel stack */
	kfree((void *) (task->kernel_stack - STACK_SIZE));

	/* exit memory */
	task_exit_mm(task);

	/* free task */
	kfree(task);
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
	return sys_fork();
}

/*
 * Clone system call.
 */
int sys_clone(uint32_t flags, uint32_t newsp)
{
	return do_fork(flags, newsp);
}

/*
 * Copy strings to binary arguments structure.
 */
static void copy_strings(struct binargs_t *bargs, char *const argv[], char *const envp[])
{
	char *str, *p = bargs->buf;
	int i;

	/* copy argv */
	for (i = 0; i < bargs->argc; i++) {
		str = argv[i];
		while (*str)
			*p++ = *str++;
		*p++ = 0;
	}

	/* copy envp */
	for (i = 0; i < bargs->envc; i++) {
		str = envp[i];
		while (*str)
			*p++ = *str++;
		*p++ = 0;
	}
}

/*
 * Init binary arguments.
 */
static int bargs_init(struct binargs_t *bargs, char *const argv[], char *const envp[])
{
	int i;

	/* reset barg */
	memset(bargs, 0, sizeof(struct binargs_t));

	/* get argc */
	for (i = 0; argv && argv[i]; i++)
		bargs->argv_len += strlen(argv[i]) + 1;
	bargs->argc = i;

	/* get envc */
	for (i = 0; envp && envp[i]; i++)
		bargs->envp_len += strlen(envp[i]) + 1;
	bargs->envc = i;

	/* check total size */
	if (bargs->argv_len + bargs->envp_len > PAGE_SIZE)
		return -ENOMEM;

	/* allocate buffer */
	bargs->buf = (char *) kmalloc(bargs->argv_len + bargs->envp_len);
	if (!bargs->buf)
		return -ENOMEM;

	return 0;
}

/*
 * Execve system call.
 */
int sys_execve(const char *path, char *const argv[], char *const envp[])
{
	struct binargs_t bargs;
	int ret;

	/* init binary arguments */
	ret = bargs_init(&bargs, argv, envp);
	if (ret)
		goto out;

	/* copy argv/envp to binary arguments structure */
	copy_strings(&bargs, argv, envp);

	/* load elf binary */
	ret = elf_load(path, &bargs);
out:
	kfree(bargs.buf);
	return ret;
}
