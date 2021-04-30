#include <x86/system.h>
#include <x86/interrupt.h>
#include <x86/tss.h>
#include <proc/sched.h>
#include <proc/task.h>
#include <proc/timer.h>
#include <list.h>
#include <stderr.h>

LIST_HEAD(tasks_list);                    /* active processes list */
static struct task_t *kinit_task;         /* kernel init task (pid = 0) */
struct task_t *current_task = NULL;       /* current task */
static pid_t next_pid = 0;                /* pid counter */

/* switch tasks (defined in scheduler.s) */
extern void scheduler_do_switch(uint32_t *current_esp, uint32_t next_esp);

/*
 * Get next tid.
 */
pid_t get_next_pid()
{
  pid_t ret;
  ret = next_pid++;
  return ret;
}

/*
 * Init scheduler.
 */
int init_scheduler(void (*kinit_func)())
{
  /* create init task */
  kinit_task = create_kernel_task(kinit_func);
  if (!kinit_task)
    return -ENOMEM;

  /* set current task to kinit */
  list_add(&kinit_task->list, &tasks_list);

  return 0;
}

/*
 * Get next task to run.
 */
static struct task_t *get_next_task()
{
  struct list_head_t *pos;
  struct task_t *task;

  /* first scheduler call : return kinit */
  if (!current_task)
    return kinit_task;

  /* get next running task */
  list_for_each(pos, &current_task->list) {
    if (pos == &tasks_list)
      continue;

    task = list_entry(pos, struct task_t, list);
    if (task->state == TASK_RUNNING)
      return task;
  }

  /* no tasks found : return current if still running */
  if (current_task->state == TASK_RUNNING)
    return current_task;

  /* else execute kinit */
  return kinit_task;
}

/*
 * Schedule function (interruptions must be disabled and will be reenabled on function return).
 */
void schedule()
{
  struct task_t *prev_task;

  /* update timers */
  timer_update();

  /* get next task to run */
  prev_task = current_task;
  current_task = get_next_task();

  /* switch tasks */
  if (prev_task != current_task) {
    tss_set_stack(0x10, current_task->kernel_stack);
    switch_page_directory(current_task->pgd);
    scheduler_do_switch(&prev_task->esp, current_task->esp);
  }
}
