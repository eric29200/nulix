#include <x86/system.h>
#include <x86/interrupt.h>
#include <x86/tss.h>
#include <proc/sched.h>
#include <proc/task.h>
#include <proc/timer.h>
#include <list.h>
#include <lock.h>
#include <stderr.h>

/* scheduler lock */
static spinlock_t sched_lock;

/* tasks list */
LIST_HEAD(tasks_ready_list);

/* first kernel task (pid = 0) */
static struct task_t *kinit_task;

/* current task */
struct task_t *current_task = NULL;

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
 * Init scheduler.
 */
int init_scheduler(void (*kinit_func)())
{
  /* init scheduler lock */
  spin_lock_init(&sched_lock);

  /* create init task */
  kinit_task = create_kernel_task(kinit_func);
  if (!kinit_task)
    return -ENOMEM;

  /* set current task to kinit */
  current_task = kinit_task;

  return run_task(kinit_task);
}

/*
 * Schedule function (interruptions must be disabled and will be reenabled on function return).
 */
void schedule()
{
  struct task_t *prev_task;
  uint32_t flags;

  /* lock scheduler and disable interrupts */
  spin_lock_irqsave(&sched_lock, flags);

  /* remember current task */
  prev_task = current_task;

  /* update timers */
  timer_update();

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
  } while (!current_task || current_task->state != TASK_RUNNING);

  /* no task : use kinit task */
  if (!current_task)
    current_task = kinit_task;

  /* unlock scheduler */
  spin_unlock(&sched_lock);

  /* switch tasks */
  tss_set_stack(0x10, current_task->kernel_stack);
  switch_page_directory(current_task->pgd);
  scheduler_do_switch(&prev_task->esp, current_task->esp);
}


/*
 * Run a task.
 */
int run_task(struct task_t *task)
{
  if (!task)
    return -EINVAL;

  /* add to the task list */
  list_add(&task->list, &tasks_ready_list);

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
  task->state = TASK_ZOMBIE;

  /* call scheduler */
  schedule();
}
