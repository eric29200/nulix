#include <x86/interrupt.h>
#include <mm/mm.h>
#include <proc/task.h>
#include <proc/sched.h>
#include <proc/elf.h>
#include <string.h>
#include <stderr.h>

/*
 * Kernel task trampoline (used to end tasks properly).
 */
static void task_entry(struct task_t *task, void (*func)(void *), void *arg)
{
  /* execute task */
  func(arg);

  /* end properly the task */
  kill_task(task);
}

typedef void (*funcc)();

/*
 * Kernel ELF task trampoline (used to end tasks properly).
 */
static void task_elf_entry(struct task_t *task, char *path)
{
  struct elf_layout_t *elf_layout;

  /* load elf header */
  elf_layout = elf_load(path);

  /* execute elf file */
  if (elf_layout)
    elf_layout->entry();

  /* end properly the task */
  kill_task(task);
  kfree(elf_layout);
  kfree(path);
}

/*
 * Create and init a task.
 */
struct task_t *create_init_task()
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
  task->pgd = clone_page_directory(kernel_pgd);

  return task;
}

/*
 * Create a task.
 */
struct task_t *create_task(void (*func)(void *), void *arg)
{
  struct task_registers_t *regs;
  struct task_t *task;

  /* create task */
  task = create_init_task();
  if (!task)
    return NULL;

  /* set registers */
  regs = (struct task_registers_t *) task->esp;
  memset(regs, 0, sizeof(struct task_registers_t));

  /* set eip to function */
  regs->parameter1 = (uint32_t) task;
  regs->parameter2 = (uint32_t) func;
  regs->parameter3 = (uint32_t) arg;
  regs->return_address = 0xFFFFFFFF;
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
struct task_t *create_elf_task(const char *path)
{
  struct task_registers_t *regs;
  struct task_t *task;

  /* create task */
  task = create_init_task();
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

  kfree((void *) (task->kernel_stack - STACK_SIZE));
  kfree(task);
}
