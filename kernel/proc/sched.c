#include <x86/system.h>
#include <x86/interrupt.h>
#include <x86/tss.h>
#include <proc/sched.h>
#include <proc/task.h>
#include <list.h>
#include <lock.h>
#include <stderr.h>

/* scheduler lock */
static spinlock_t sched_lock;

/* tasks list */
LIST_HEAD(tasks_ready_list);
LIST_HEAD(tasks_waiting_list);
struct task_t *current_task = NULL;
struct task_t *idle_task = NULL;

/* tids counter */
static pid_t next_pid = 0;

/* switch tasks (defined in scheduler.s) */
extern void scheduler_do_switch(uint32_t *current_esp, uint32_t next_esp);

/*
 * Get next tid.
 */
pid_t get_next_pid()
{
  uint32_t flags;
  pid_t ret;

  spin_lock_irqsave(&sched_lock, flags);
  ret = next_pid++;
  spin_unlock_irqrestore(&sched_lock, flags);

  return ret;
}

/*
 * Idle task (used if no tasks are ready).
 */
void idle_func()
{
  for (;;)
    halt();
}

/*
 * Init scheduler.
 */
int init_scheduler(void (*init_func)())
{
  struct task_t *init_task;

  /* init scheduler lock */
  spin_lock_init(&sched_lock);

  /* create idle task */
  idle_task = create_kernel_task(idle_func);
  if (!idle_task)
    return -ENOMEM;

  /* create init task */
  init_task = create_kernel_task(init_func);
  if (!init_task)
    return -ENOMEM;

  return run_task(init_task);
}

/*
 * Update task state.
 */
static void __update_task_state(struct task_t *task, uint8_t state)
{
  uint32_t flags;

  /* update state */
  task->state = state;

  /* updates tasks list */
  spin_lock_irqsave(&sched_lock, flags);
  list_del(&task->list);
  if (task->state == TASK_READY)
    list_add(&task->list, &tasks_ready_list);
  else if (task->state == TASK_WAITING)
    list_add(&task->list, &tasks_waiting_list);
  spin_unlock_irqrestore(&sched_lock, flags);
}

/*
 * Schedule function (interruptions must be disabled and will be reenabled on function return).
 */
void schedule()
{
  struct task_t *prev_task, *task;
  struct list_head_t *pos, *n;
  uint32_t flags;

  /* remember current task */
  prev_task = current_task;

  /* lock scheduler and disable interrupts */
  spin_lock_irqsave(&sched_lock, flags);

  /* if current task is terminated : destroy it */
  if (current_task->state == TASK_TERMINATED) {
    destroy_task(current_task);
    current_task = NULL;
  }

  /* update waiting tasks */
  list_for_each_safe(pos, n, &tasks_waiting_list) {
    task = list_entry(pos, struct task_t, list);

    /* timer expires : remove it from waiting list and run it */
    if (--task->expires == 0) {
      list_del(&task->list);
      task->state = TASK_READY;
      list_add(&task->list, &tasks_ready_list);
    }
  }

  /* scheduling algorithm */
  do {
    /* no tasks : break */
    if (list_empty(&tasks_ready_list)) {
      current_task = NULL;
      break;
    }

    /* pop next task */
    current_task = list_first_entry(&tasks_ready_list, struct task_t, list);
    list_del(&current_task->list);

    /* put current task at the end of the list */
    list_add_tail(&current_task->list, &tasks_ready_list);
  } while (!current_task);

  /* no task : use idle task */
  if (!current_task)
    current_task = idle_task;

  /* unlock scheduler */
  spin_unlock(&sched_lock);

  /* switch tasks */
  if (current_task != prev_task) {
    /* set tss */
    tss_set_stack(0x10, current_task->kernel_stack);

    /* switch page directory */
    switch_page_directory(current_task->pgd);

    /* switch */
    scheduler_do_switch(&prev_task->esp, current_task->esp);
  }

  /* restore irq */
  irq_enable();
}

/*
 * Put current task in sleep mode for timeout jiffies.
 */
void schedule_timeout(uint32_t timeout)
{
  if (current_task) {
    /* set timeout */
    current_task->expires = timeout;
    current_task->state = TASK_WAITING;

    /* put current task in waiting list */
    spin_lock(&sched_lock);
    list_del(&current_task->list);
    list_add(&current_task->list, &tasks_waiting_list);
    spin_unlock(&sched_lock);
  }

  /* reschedule */
  schedule();
}

/*
 * Run a task.
 */
int run_task(struct task_t *task)
{
  if (!task)
    return -EINVAL;

  /* add to the task list */
  __update_task_state(task, TASK_READY);

  return 0;
}

/*
 * Kill a task.
 */
void kill_task(struct task_t *task)
{
  /* NULL task */
  if (!task)
    return;

  /* mark task terminated and reschedule */
  __update_task_state(task, TASK_TERMINATED);

  /* call scheduler */
  schedule();
}
