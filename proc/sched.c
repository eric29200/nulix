#include <x86/system.h>
#include <x86/interrupt.h>
#include <proc/sched.h>
#include <proc/task.h>
#include <list.h>
#include <stderr.h>

/* threads list (idle thread is always the last thread in the list) */
LIST_HEAD(threads_list);
static struct thread_t *current_thread = NULL;
static struct thread_t *idle_thread = NULL;

/* switch threads (defined in scheduler.s) */
extern void scheduler_do_switch(uint32_t *current_esp, uint32_t next_esp);

/*
 * Idle task.
 */
static void idle_task()
{
  for (;;)
    halt();
}

/*
 * Init scheduler.
 */
int init_scheduler()
{
  uint32_t flags;

  /* create idle thread */
  idle_thread = create_thread(idle_task);
  if (!idle_thread)
    return ENOMEM;

  /* add it to the threads list */
  irq_save(flags);
  list_add(&idle_thread->list, &threads_list);
  current_thread = idle_thread;
  irq_restore(flags);

  return 0;
}

/*
 * Schedule function.
 */
void schedule()
{
  struct thread_t *prev_thread = current_thread;

  /* take first thread */
  current_thread = list_first_entry(&threads_list, struct thread_t, list);

  /* put it at the end of the list (just before idle thread) */
  if (current_thread != idle_thread) {
    list_del(&current_thread->list);
    list_add_tail(&current_thread->list, &idle_thread->list);
  }

  /* switch threads */
  if (current_thread != prev_thread)
    scheduler_do_switch(&prev_thread->esp, current_thread->esp);
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
