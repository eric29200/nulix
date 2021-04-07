#include <x86/interrupt.h>
#include <mm/mm.h>
#include <proc/task.h>
#include <proc/sched.h>
#include <string.h>
#include <stderr.h>

/* tids counter */
static uint32_t next_tid = 0;

/*
 * Destroy a thread.
 */
static void thread_destroy(struct thread_t *thread)
{
  if (!thread)
    return;

  kfree((void *) (thread->kernel_stack - STACK_SIZE));
  kfree(thread);
}


/*
 * Kernel thread trampoline (used to end threads properly).
 */
static void thread_entry(struct thread_t *thread, void (*func)())
{
  /* execute thread */
  func();

  /* remove thread from the list and destroy it */
  irq_disable();
  list_del(&thread->list);
  thread_destroy(thread);

  /* reschedule */
  schedule();
}

/*
 * Create a thread.
 */
struct thread_t *create_thread(void (*func)(void))
{
  struct task_registers_t *regs;
  struct thread_t *thread;
  void *stack;

  /* create thread */
  thread = (struct thread_t *) kmalloc(sizeof(struct thread_t));
  if (!thread)
    return NULL;

  /* set tid */
  thread->tid = next_tid++;
  INIT_LIST_HEAD(&thread->list);

  /* allocate stack */
  stack = (void *) kmalloc(STACK_SIZE);
  if (!stack) {
    kfree(thread);
    return NULL;
  }

  /* set stack */
  memset(stack, 0, STACK_SIZE);
  thread->kernel_stack = (uint32_t) stack + STACK_SIZE;
  thread->esp = thread->kernel_stack - sizeof(struct task_registers_t);

  /* set registers */
  regs = (struct task_registers_t *) thread->esp;
  memset(regs, 0, sizeof(struct task_registers_t));

  /* set eip to function */
  regs->parameter1 = (uint32_t) thread;
  regs->parameter2 = (uint32_t) func;
  regs->return_address = 0xFFFFFFFF;
  regs->eip = (uint32_t) thread_entry;

  return thread;
}
