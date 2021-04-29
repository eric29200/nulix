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
 * Kernel ELF task trampoline (used to end tasks properly).
 */
static void task_elf_entry(struct task_t *task, char *path)
{
  /* load elf header */
  task->path = path;
  if (elf_load(path) == 0)
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

  /* set tid */
  task->pid = get_next_pid();
  task->state = TASK_RUNNING;
  task->path = NULL;
  task->user_stack_size = 0;
  task->start_brk = 0;
  task->end_brk = 0;
  task->parent = parent;
  INIT_LIST_HEAD(&task->list);
  INIT_LIST_HEAD(&task->children);
  INIT_LIST_HEAD(&task->sibling);

  /* init open files */
  for (i = 0; i < NR_OPEN; i++)
    task->filp[i] = NULL;

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

  return task;
}

/*
 * Create a task.
 */
struct task_t *create_kernel_task(void (*func)(void))
{
  struct task_registers_t *regs;
  struct task_t *task;

  /* create task */
  task = create_task(NULL);
  if (!task)
    return NULL;

  /* use kernel page directory */
  task->pgd = kernel_pgd;

  /* set registers */
  regs = (struct task_registers_t *) task->esp;
  memset(regs, 0, sizeof(struct task_registers_t));

  /* set eip to function */
  regs->return_address = TASK_RETURN_ADDRESS;
  regs->eip = (uint32_t) func;
  regs->eax = 0;
  regs->ecx = 0;
  regs->edx = 0;
  regs->ebx = 0;
  regs->esp = 0;
  regs->ebp = 0;
  regs->esi = 0;
  regs->edi = 0;

  return task;
}

/*
 * Fork a task.
 */
struct task_t *fork_task(struct task_t *parent)
{
  struct task_registers_t *regs;
  struct task_t *task;
  int i;

  /* create task */
  task = create_task(parent);
  if (!task)
    return NULL;

  /* duplicate page directory */
  task->pgd = clone_page_directory(parent->pgd);

  /* add child to parent */
  list_add(&task->sibling, &parent->children);

  /* copy open files */
  for (i = 0; i < NR_OPEN; i++) {
    task->filp[i] = parent->filp[i];
    if (task->filp[i])
      task->filp[i]->f_ref++;
  }

  /* set user stack to parent */
  task->user_stack = parent->user_stack;
  task->user_stack_size = parent->user_stack_size;

  /* set user mapped memory to parent */
  task->start_brk = parent->start_brk;
  task->end_brk = parent->end_brk;

  /* duplicate parent registers */
  memcpy(&task->user_regs, &parent->user_regs, sizeof(struct registers_t));
  task->user_regs.eax = 0;

  /* set registers */
  regs = (struct task_registers_t *) task->esp;
  memset(regs, 0, sizeof(struct task_registers_t));

  /* set eip to function */
  regs->parameter1 = (uint32_t) task;
  regs->return_address = TASK_RETURN_ADDRESS;
  regs->eip = (uint32_t) task_user_entry;
  regs->eax = 0;
  regs->ecx = 0;
  regs->edx = 0;
  regs->ebx = 0;
  regs->esp = 0;
  regs->ebp = 0;
  regs->esi = 0;
  regs->edi = 0;

  return task;
}

/*
 * Spawn init process.
 */
int spawn_init()
{
  struct task_registers_t *regs;
  struct task_t *task;

  /* create task */
  task = create_task(NULL);
  if (!task)
    return -ENOMEM;

  /* clone page directory */
  task->pgd = clone_page_directory(current_task->pgd);

  /* set brk to end of user stack */
  task->start_brk = task->user_stack + PAGE_SIZE;
  task->end_brk = task->user_stack + PAGE_SIZE;

  /* set registers */
  regs = (struct task_registers_t *) task->esp;
  memset(regs, 0, sizeof(struct task_registers_t));

  /* set eip */
  regs->parameter1 = (uint32_t) task;
  regs->parameter2 = (uint32_t) strdup("/sbin/init");
  regs->return_address = TASK_RETURN_ADDRESS;
  regs->eip = (uint32_t) task_elf_entry;
  regs->eax = 0;
  regs->ecx = 0;
  regs->edx = 0;
  regs->ebx = 0;
  regs->esp = 0;
  regs->ebp = 0;
  regs->esi = 0;
  regs->edi = 0;

  /* add task */
  list_add(&task->list, &tasks_list);

  return 0;
}

/*
 * Destroy a task.
 */
void destroy_task(struct task_t *task)
{
  if (!task)
    return;

  /* free task path */
  if (task->path)
    kfree(task->path);

  /* free kernel stack */
  kfree((void *) (task->kernel_stack - STACK_SIZE));

  /* free page directory */
  if (task->pgd != kernel_pgd)
    free_page_directory(task->pgd);

  /* free task */
  kfree(task);
}
