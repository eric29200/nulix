#include <kernel/task.h>
#include <kernel/mm.h>
#include <kernel/interrupt.h>
#include <string.h>
#include <stderr.h>

/* threads list */
LIST_HEAD(threads_list);
static struct thread_t *current_thread = NULL;

/* tids counter */
static uint32_t next_tid = 0;

/* switch threads (defined in scheduler.s) */
extern void do_switch(uint32_t *current_esp, uint32_t next_esp);

/*
 * Schedule function.
 */
void schedule()
{
  struct thread_t *prev_thread = current_thread;

  /* take next thread or first one */
  if (current_thread && !list_is_last(&current_thread->list, &threads_list))
    current_thread = list_next_entry(current_thread, list);
  else if (!list_empty(&threads_list))
    current_thread = list_first_entry(&threads_list, struct thread_t, list);

  /* switch threads */
  if (current_thread)
    do_switch(&prev_thread->esp, current_thread->esp);
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
  regs->eip = (uint32_t) func;

  return thread;
}

/*
 * Start a thread.
 */
int start_thread(void (*func)(void))
{
  struct thread_t *thread;
  uint32_t flags;

  /* create a thread */
  thread = create_thread(func);
  if (!thread)
    return ENOMEM;

  /* add to the threads list */
  irq_save(flags);
  list_add(&thread->list, &threads_list);
  irq_restore(flags);

  return 0;
}
