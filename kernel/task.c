#include <kernel/task.h>
#include <kernel/mm.h>
#include <string.h>
#include <stderr.h>

/* tids counter */
static uint32_t next_tid = 0;

/* threads list */
static struct thread_t *threads = NULL;

/* current thread */
static struct thread_t *current_thread = NULL;

/* switch threads (defined in scheduler.s) */
extern void do_switch(uint32_t *current_esp, uint32_t next_esp);

/*
 * Schedule function.
 */
void schedule()
{
  struct thread_t *prev_thread = current_thread;

  /* take next thread */
  if (current_thread)
    current_thread = current_thread->next;

  /* or first one */
  if (!current_thread)
    current_thread = threads;

  if (!prev_thread)
    prev_thread = current_thread;

  /* switch threads */
  if (current_thread)
    do_switch(&prev_thread->esp, current_thread->esp);
}

/*
 * Create a thread.
 */
int create_thread(void (*func)(void))
{
  struct task_registers_t *regs;
  struct thread_t *thread, *t;
  void *stack;

  /* create thread */
  thread = (struct thread_t *) kmalloc(sizeof(struct thread_t));
  if (!thread)
    return ENOMEM;

  /* set tid */
  thread->tid = next_tid++;
  thread->next = NULL;

  /* allocate stack */
  stack = (void *) kmalloc(STACK_SIZE);
  if (!stack) {
    kfree(thread);
    return ENOMEM;
  }

  /* set stack */
  memset(stack, 0, STACK_SIZE);
  thread->kernel_stack = (uint32_t) stack + STACK_SIZE;
  thread->esp = thread->kernel_stack - sizeof(struct task_registers_t);

  /* set registers */
  regs = (struct task_registers_t *) thread->esp;
  memset(regs, 0, sizeof(struct task_registers_t));

  /* set eip to function */
  regs->eip = (uint32_t) func;

  /* add to the tail of the threads */
  for (t = threads; t != NULL && t->next != NULL; t = t->next);
  if (t)
    t->next = thread;
  else
    threads = thread;

  return 0;
}
