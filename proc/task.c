#include <x86/interrupt.h>
#include <mm/mm.h>
#include <proc/task.h>
#include <proc/sched.h>
#include <string.h>
#include <stderr.h>

/* tids counter */
static uint32_t next_tid = 0;

/*
 * Kernel task trampoline (used to end tasks properly).
 */
static void task_entry(struct task_t *task, void (*func)())
{
  /* execute task */
  func();

  /* end properly the task */
  kill_task(task);
}

/*
 * Create a task.
 */
struct task_t *create_task(void (*func)(void))
{
  struct task_registers_t *regs;
  struct task_t *task;
  void *stack;

  /* create task */
  task = (struct task_t *) kmalloc(sizeof(struct task_t));
  if (!task)
    return NULL;

  /* set tid */
  task->tid = next_tid++;
  task->state = TASK_NEW;
  INIT_LIST_HEAD(&task->list);

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

  /* set registers */
  regs = (struct task_registers_t *) task->esp;
  memset(regs, 0, sizeof(struct task_registers_t));

  /* set eip to function */
  regs->parameter1 = (uint32_t) task;
  regs->parameter2 = (uint32_t) func;
  regs->return_address = 0xFFFFFFFF;
  regs->eip = (uint32_t) task_entry;

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
