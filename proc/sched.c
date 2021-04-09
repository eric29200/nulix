#include <x86/system.h>
#include <x86/interrupt.h>
#include <proc/sched.h>
#include <proc/task.h>
#include <proc/timer.h>
#include <list.h>
#include <lock.h>
#include <stderr.h>

/* tasks list */
LIST_HEAD(tasks_list);
static struct task_t *current_task = NULL;
static struct task_t *idle_task = NULL;

/* timers list */
LIST_HEAD(timers_list);

/* scheduler lock */
struct spinlock_t sched_lock;

/* switch tasks (defined in scheduler.s) */
extern void scheduler_do_switch(uint32_t *current_esp, uint32_t next_esp);

/*
 * Idle task (used if no tasks are ready).
 */
void idle_task_func(void *arg)
{
  UNUSED(arg);

  for (;;)
    halt();
}

/*
 * Init scheduler.
 */
int init_scheduler(void (*init_func)(void *), void *init_arg)
{
  struct task_t *init_task;

  /* init scheduler lock */
  spin_lock_init(&sched_lock);

  /* create idle task */
  idle_task = create_task(idle_task_func, NULL);
  if (!idle_task)
    return ENOMEM;

  /* create init task */
  init_task = create_task(init_func, init_arg);
  if (!init_task) {
    destroy_task(idle_task);
    return ENOMEM;
  }

  /* run init task */
  return run_task(init_task);
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
  struct list_head_t *pos, *n;
  struct task_t *prev_task;
  struct timer_t *timer;

  /* disable interrupts */
  irq_disable();

  /* remember current task */
  prev_task = current_task;

  /* update timers expires */
  list_for_each_safe(pos, n, &timers_list) {
    timer = list_entry(pos, struct timer_t, list);
    timer->expires--;

    /* timer expires : run it */
    if (timer->expires <= 0) {
      run_task(timer->task);
      list_del(&timer->list);
    }
  }

  /* scheduling algorithm */
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
      continue;
    }

    /* put current task at the end of the list */
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
 * Run a task.
 */
int run_task(struct task_t *task)
{
  uint32_t flags;

  if (!task)
    return EINVAL;

  spin_lock_irqsave(&sched_lock, flags);

  /* add to the task list */
  if (task->state == TASK_NEW)
    list_add(&task->list, &tasks_list);

  /* set running state */
  task->state = TASK_READY;

  spin_unlock_irqrestore(&sched_lock, flags);

  /* reschedule */
  schedule();

  return 0;
}

/*
 * Kill a task.
 */
void kill_task(struct task_t *task)
{
  uint32_t flags;

  /* NULL task */
  if (!task)
    return;

  /* mark task terminated and reschedule */
  spin_lock_irqsave(&sched_lock, flags);
  task->state = TASK_TERMINATED;
  spin_unlock_irqrestore(&sched_lock, flags);

  /* call scheduler */
  schedule();
}

/*
 * Run a timer.
 */
int run_timer(struct timer_t *timer)
{
  uint32_t flags;

  if (!timer)
    return EINVAL;

  /* add to the timer list */
  spin_lock_irqsave(&sched_lock, flags);
  list_add(&timer->list, &timers_list);
  spin_unlock_irqrestore(&sched_lock, flags);

  return 0;
}
