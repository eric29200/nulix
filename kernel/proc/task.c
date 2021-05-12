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
  /* load elf header */
  if (elf_load("/sbin/init") == 0)
    enter_user_mode(task->user_stack, task->user_entry, TASK_RETURN_ADDRESS);
}

/*
 * Create and init a task.
 */
static struct task_t *create_task(struct task_t *parent)
{
  struct task_t *task;
  void *stack;
  int i;

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
  task->gid = parent ? parent->gid : 0;
  task->umask = parent ? parent->umask : 0022;
  task->user_stack = parent ? parent->user_stack : 0;
  task->start_text = parent ? parent->start_text : 0;
  task->end_text = parent ? parent->end_text : 0;
  task->start_brk = parent ? parent->start_brk : 0;
  task->end_brk = parent ? parent->end_brk : 0;
  INIT_LIST_HEAD(&task->list);

  /* clone page directory */
  task->pgd = clone_page_directory(parent ? parent->pgd : kernel_pgd);
  if (!task->pgd) {
    kfree(stack);
    kfree(task);
    return NULL;
  }

  /* duplicate parent registers */
  if (parent) {
    memcpy(&task->user_regs, &parent->user_regs, sizeof(struct registers_t));
    task->user_regs.eax = 0;
  }

  /* duplicate current working dir */
  if (parent && parent->cwd) {
    task->cwd = parent->cwd;
    task->cwd->i_ref++;
  } else {
    task->cwd = NULL;
  }

  /* copy open files */
  for (i = 0; i < NR_OPEN; i++) {
    task->filp[i] = parent ? parent->filp[i] : NULL;
    if (task->filp[i])
      task->filp[i]->f_ref++;
  }

  return task;
}

/*
 * Create kernel init task.
 */
struct task_t *create_kinit_task(void (*func)(void))
{
  struct task_registers_t *regs;
  struct task_t *task;

  /* create task */
  task = create_task(NULL);
  if (!task)
    return NULL;

  /* set registers */
  regs = (struct task_registers_t *) task->esp;
  memset(regs, 0, sizeof(struct task_registers_t));

  /* set eip to function */
  regs->return_address = TASK_RETURN_ADDRESS;
  regs->eip = (uint32_t) func;

  return task;
}

/*
 * Fork a task.
 */
struct task_t *fork_task(struct task_t *parent)
{
  struct task_registers_t *regs;
  struct task_t *task;

  /* create task */
  task = create_task(parent);
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
struct task_t *create_init_task()
{
  struct task_registers_t *regs;
  struct task_t *task;

  /* create task */
  task = create_task(NULL);
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

  /* free page directory */
  if (task->pgd != kernel_pgd)
    free_page_directory(task->pgd);

  /* free task */
  kfree(task);
}
