#include <x86/system.h>
#include <x86/interrupt.h>
#include <proc/sched.h>
#include <proc/task.h>
#include <list.h>
#include <lock.h>
#include <stderr.h>

/* tasks list */
LIST_HEAD(tasks_list);
static struct task_t *current_task = NULL;
static struct task_t *idle_task = NULL;

/* scheduler lock */
struct spinlock_t sched_lock;

/* switch tasks (defined in scheduler.s) */
extern void scheduler_do_switch(uint32_t *current_esp, uint32_t next_esp);

/*
 * Idle task (used if no tasks are ready).
 */
void idle_task_func()
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
  idle_task = create_task(idle_task_func);
  if (!idle_task)
    return ENOMEM;

  /* create init task */
  ret = start_task(init_func);
  if (ret != 0)
    destroy_task(idle_task);

  return ret;
}

/*
 * Pop next task to run.
 */
static struct task_t *pop_next_task()
{
  struct task_t *next_task;

  next_task = list_first_entry(&tasks_list, struct task_t, list);
  list_del(&next_task->list);

  return next_task;
}

/*
 * Schedule function (interruptions must be disabled and will be reenabled on function return).
 */
void schedule()
{
  struct task_t *prev_task;

  /* disable interrupts */
  irq_disable();

  /* remember current task */
  prev_task = current_task;

  do {
    /* no tasks : break */
    if (list_empty(&tasks_list)) {
      current_task = NULL;
      break;
    }

    /* pop next task */
    current_task = pop_next_task();

    /* if task is terminated : destroy it */
    if (current_task->state == TASK_TERMINATED) {
      current_task = NULL;
      destroy_task(current_task);
      continue;
    }

    /* put current task at thed end of the list */
    list_add_tail(&current_task->list, &tasks_list);
  } while (!current_task);

  /* no running task : use idle task */
  if (!current_task)
    current_task = idle_task;

  /* switch tasks */
  if (current_task != prev_task)
    scheduler_do_switch(&prev_task->esp, current_task->esp);
}

/*
 * Start a task.
 */
int start_task(void (*func)(void))
{
  struct task_t *task;
  uint32_t flags;

  /* create a task */
  task = create_task(func);
  if (!task)
    return ENOMEM;

  /* add to the tasks list */
  spin_lock_irqsave(&sched_lock, flags);
  list_add(&task->list, &tasks_list);
  spin_unlock_irqrestore(&sched_lock, flags);

  return 0;
}

/*
 * End a task.
 */
void end_task(struct task_t *task)
{
  uint32_t flags;

  /* mark task terminated and reschedule */
  spin_lock_irqsave(&sched_lock, flags);
  task->state = TASK_TERMINATED;
  spin_unlock_irqrestore(&sched_lock, flags);

  /* call scheduler */
  schedule();
}
