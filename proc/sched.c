#include <x86/system.h>
#include <x86/interrupt.h>
#include <proc/sched.h>
#include <proc/task.h>
#include <list.h>
#include <lock.h>
#include <stderr.h>

/* threads list */
LIST_HEAD(threads_list);
static struct thread_t *current_thread = NULL;
static struct thread_t *idle_thread = NULL;

/* scheduler lock */
struct spinlock_t sched_lock;

/* switch threads (defined in scheduler.s) */
extern void scheduler_do_switch(uint32_t *current_esp, uint32_t next_esp);

/*
 * Idle task (used if no threads are ready).
 */
void idle_task()
{
  for (;;)
    halt();
}

/*
 * Init scheduler.
 */
int init_scheduler(void (*init_func)(void))
{
  int ret;

  /* init scheduler lock */
  spin_lock_init(&sched_lock);

  /* create idle task */
  idle_thread = create_thread(idle_task);
  if (!idle_thread)
    return ENOMEM;

  /* create init thread */
  ret = start_thread(init_func);
  if (ret != 0)
    destroy_thread(idle_thread);

  return ret;
}

/*
 * Pop next thread to run.
 */
static struct thread_t *pop_next_thread()
{
  struct thread_t *next_thread;

  next_thread = list_first_entry(&threads_list, struct thread_t, list);
  list_del(&next_thread->list);

  return next_thread;
}

/*
 * Schedule function (interruptions must be disabled and will be reenabled on function return).
 */
void schedule()
{
  struct thread_t *prev_thread = current_thread;

  do {
    /* no threads : break */
    if (list_empty(&threads_list)) {
      current_thread = NULL;
      break;
    }

    /* pop next thread */
    current_thread = pop_next_thread();

    /* if thread is terminated : destroy it */
    if (current_thread->state == THREAD_TERMINATED) {
      current_thread = NULL;
      destroy_thread(current_thread);
      continue;
    }

    /* put current thread at thed end of the list */
    list_add_tail(&current_thread->list, &threads_list);
  } while (!current_thread);

  /* no running thread : use idle thread */
  if (!current_thread)
    current_thread = idle_thread;

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
  spin_lock_irqsave(&sched_lock, flags);
  list_add(&thread->list, &threads_list);
  spin_unlock_irqrestore(&sched_lock, flags);

  return 0;
}

/*
 * End a thread.
 */
void end_thread(struct thread_t *thread)
{
  uint32_t flags;

  /* lock scheduler */
  spin_lock_irqsave(&sched_lock, flags);

  /* mark thread terminated and reschedule */
  thread->state = THREAD_TERMINATED;

  /* call scheduler */
  schedule();
}
