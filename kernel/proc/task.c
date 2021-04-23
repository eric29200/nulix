#include <x86/interrupt.h>
#include <mm/mm.h>
#include <proc/task.h>
#include <proc/sched.h>
#include <proc/elf.h>
#include <string.h>
#include <stderr.h>

/* switch to user mode (defined in x86/scheduler.s) */
extern void enter_user_mode(uint32_t esp, uint32_t eip, uint32_t return_address);

/*
 * Kernel task trampoline (used to end tasks properly).
 */
static void task_entry(void (*func)(void *), void *arg)
{
  func(arg);
}

/*
 * Kernel ELF task trampoline (used to end tasks properly).
 */
static void task_elf_entry(struct task_t *task, char *path)
{
  /* load elf header */
  task->elf_layout = elf_load(path);
  task->path = path;

  /* execute elf file */
  if (task->elf_layout)
    enter_user_mode(task->elf_layout->stack, task->elf_layout->entry, TASK_RETURN_ADDRESS);
}

/*
 * Create and init a task.
 */
struct task_t *create_init_task(uint8_t kernel)
{
  struct task_t *task;
  void *stack;
  int i;

  /* create task */
  task = (struct task_t *) kmalloc(sizeof(struct task_t));
  if (!task)
    return NULL;

  /* set tid */
  task->tid = get_next_tid();
  task->state = TASK_NEW;
  task->expires = 0;
  task->path = NULL;
  task->elf_layout = NULL;
  INIT_LIST_HEAD(&task->list);

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

  /* create a new page directory */
  if (kernel)
    task->pgd = kernel_pgd;
  else
    task->pgd = clone_page_directory(kernel_pgd);

  return task;
}

/*
 * Create a task.
 */
struct task_t *create_kernel_task(void (*func)(void *), void *arg)
{
  struct task_registers_t *regs;
  struct task_t *task;

  /* create task */
  task = create_init_task(1);
  if (!task)
    return NULL;

  /* set registers */
  regs = (struct task_registers_t *) task->esp;
  memset(regs, 0, sizeof(struct task_registers_t));

  /* set eip to function */
  regs->parameter1 = (uint32_t) func;
  regs->parameter2 = (uint32_t) arg;
  regs->return_address = TASK_RETURN_ADDRESS;
  regs->eip = (uint32_t) task_entry;
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
 * Create an ELF task.
 */
struct task_t *create_user_elf_task(const char *path)
{
  struct task_registers_t *regs;
  struct task_t *task;

  /* create task */
  task = create_init_task(0);
  if (!task)
    return NULL;

  /* set registers */
  regs = (struct task_registers_t *) task->esp;
  memset(regs, 0, sizeof(struct task_registers_t));

  /* set eip */
  regs->parameter1 = (uint32_t) task;
  regs->parameter2 = (uint32_t) strdup(path);
  regs->return_address = 0xFFFFFFFF;
  regs->eip = (uint32_t) task_elf_entry;
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
 * Destroy a task.
 */
void destroy_task(struct task_t *task)
{
  if (!task)
    return;

  /* free task path */
  if (task->path)
    kfree(task->path);

  /* free ELF layout */
  if (task->elf_layout)
    kfree(task->elf_layout);

  /* free kernel stack */
  kfree((void *) (task->kernel_stack - STACK_SIZE));

  /* free page directory */
  if (task->pgd != kernel_pgd)
    free_page_directory(task->pgd);

  /* free task */
  kfree(task);
}
