#include <x86/interrupt.h>
#include <x86/tss.h>
#include <mm/mm.h>
#include <proc/task.h>
#include <proc/sched.h>
#include <proc/elf.h>
#include <string.h>
#include <stderr.h>

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
	struct binargs_t bargs = {
		.buf		= NULL,
		.argc		= 0,
		.argv_len	= 0,
		.envc		= 0,
		.envp_len	= 0,
	};

	/* load elf header */
	if (elf_load("/sbin/init", &bargs) == 0)
		enter_user_mode(task->user_stack, task->user_entry, TASK_RETURN_ADDRESS);
}

/*
 * Copy signal handlers.
 */
static int task_copy_signals(struct task_t *task, struct task_t *parent)
{
	/* init signals */
	sigemptyset(&task->sigpend);
	sigemptyset(&task->sigmask);

	/* copy signals */
	if (parent)
		memcpy(task->signals, parent->signals, sizeof(task->signals));

	return 0;
}

/*
 * Copy page directory.
 */
static int task_copy_pgd(struct task_t *task, struct task_t *parent)
{
	/* clone page directory */
	task->pgd = clone_page_directory(parent ? parent->pgd : kernel_pgd);
	if (!task->pgd)
		return -ENOMEM;

	return 0;
}

/*
 * Copy memory areas.
 */
static int task_copy_mm(struct task_t *task, struct task_t *parent)
{
	struct vm_area_t *vm_parent, *vm_child;
	struct list_head_t *pos;

	/* copy virtual memory areas */
	if (parent) {
		list_for_each(pos, &parent->vm_list) {
			vm_parent = list_entry(pos, struct vm_area_t, list);
			vm_child = (struct vm_area_t *) kmalloc(sizeof(struct vm_area_t));
			if (!vm_child)
				return -ENOMEM;

			vm_child->vm_start = vm_parent->vm_start;
			vm_child->vm_end = vm_parent->vm_end;
			vm_child->vm_flags = vm_parent->vm_flags;
			list_add_tail(&vm_child->list, &task->vm_list);
		}
	}

	return 0;
}

/*
 * Copy file system informations.
 */
static int task_copy_fs(struct task_t *task, struct task_t *parent)
{
	/* duplicate current working dir */
	if (parent && parent->cwd) {
		task->cwd = parent->cwd;
		task->cwd->i_ref++;
	} else {
		task->cwd = NULL;
	}

	/* duplicate root dir */
	if (parent && parent->root) {
		task->root = parent->root;
		task->root->i_ref++;
	} else {
		task->root = NULL;
	}

	return 0;
}

/*
 * Copy files.
 */
static int task_copy_files(struct task_t *task, struct task_t *parent)
{
	int i;

	/* copy open files */
	for (i = 0; i < NR_OPEN; i++) {
		task->filp[i] = parent ? parent->filp[i] : NULL;
		if (task->filp[i])
			task->filp[i]->f_ref++;
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
 * Create and init a task.
 */
static struct task_t *create_task(struct task_t *parent, uint32_t user_sp)
{
	struct task_t *task;
	void *stack;

	/* create task */
	task = (struct task_t *) kmalloc(sizeof(struct task_t));
	if (!task)
		return NULL;

	/* allocate stack */
	stack = (void *) kmalloc(STACK_SIZE);
	if (!stack) {
		kfree(task);
		return NULL;
	}

	/* set stack */
	memset(stack, 0, STACK_SIZE);
	task->kernel_stack = (uint32_t) stack + STACK_SIZE;
	task->esp = task->kernel_stack - sizeof(struct task_registers_t);

	/* init task */
	task->pid = get_next_pid();
	task->pgid = parent ? parent->pgid : task->pid;
	task->state = TASK_RUNNING;
	task->parent = parent;
	task->uid = parent ? parent->uid : 0;
	task->euid = parent ? parent->euid : 0;
	task->suid = parent ? parent->euid : 0;
	task->gid = parent ? parent->gid : 0;
	task->egid = parent ? parent->egid : 0;
	task->sgid = parent ? parent->sgid : 0;
	task->umask = parent ? parent->umask : 0022;
	task->tty = parent ? parent->tty : 0;
	task->user_stack = parent ? parent->user_stack : 0;
	task->start_text = parent ? parent->start_text : 0;
	task->end_text = parent ? parent->end_text : 0;
	task->start_brk = parent ? parent->start_brk : 0;
	task->end_brk = parent ? parent->end_brk : 0;
	task->timeout = 0;
	task->utime = 0;
	task->stime = 0;
	task->cutime = 0;
	task->cstime = 0;
	task->start_time = jiffies;
	task->wait_child_exit = NULL;
	INIT_LIST_HEAD(&task->list);
	INIT_LIST_HEAD(&task->vm_list);

	/* copy task name and TLS */
	if (parent) {
		memcpy(task->name, parent->name, TASK_NAME_LEN);
		memcpy(&task->tls, &parent->tls, sizeof(struct user_desc_t));
	} else {
		memset(task->name, 0, TASK_NAME_LEN);
		memset(&task->tls, 0, sizeof(struct user_desc_t));
	}

	/* copy task */
	if (task_copy_pgd(task, parent))
		goto err;
	if (task_copy_mm(task, parent))
		goto err;
	if (task_copy_fs(task, parent))
		goto err;
	if (task_copy_files(task, parent))
		goto err;
	if (task_copy_signals(task, parent))
		goto err;
	if (task_copy_thread(task, parent, user_sp))
		goto err;

	return task;
err:
	destroy_task(task);
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
	task = create_task(NULL, 0);
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
struct task_t *fork_task(struct task_t *parent, uint32_t user_sp)
{
	struct task_registers_t *regs;
	struct task_t *task;

	/* create task */
	task = create_task(parent, user_sp);
	if (!task)
		return NULL;

	/* set registers */
	regs = (struct task_registers_t *) task->esp;
	memset(regs, 0, sizeof(struct task_registers_t));

	/* set eip to function */
	regs->parameter1 = (uint32_t) task;
	regs->return_address = TASK_RETURN_ADDRESS;
	regs->eip = (uint32_t) task_user_entry;

	return task;
}
/*
 * Create init process.
 */
struct task_t *create_init_task(struct task_t *parent)
{
	struct task_registers_t *regs;
	struct task_t *task;

	/* create task */
	task = create_task(parent, 0);
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
	struct list_head_t *pos, *n;
	struct vm_area_t *vm_area;

	if (!task)
		return;

	/* remove task */
	list_del(&task->list);

	/* free kernel stack */
	kfree((void *) (task->kernel_stack - STACK_SIZE));

	/* free memory regions */
	list_for_each_safe(pos, n, &task->vm_list) {
		vm_area = list_entry(pos, struct vm_area_t, list);
		if (vm_area) {
			unmap_pages(vm_area->vm_start, vm_area->vm_end, task->pgd);
			list_del(&vm_area->list);
			kfree(vm_area);
		}
	}

	/* free page directory */
	if (task->pgd != kernel_pgd)
		free_page_directory(task->pgd);

	/* free task */
	kfree(task);
}
