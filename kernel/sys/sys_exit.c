#include <sys/syscall.h>
#include <proc/sched.h>

/*
 * Exit a task.
 */
void sys_exit(int status)
{
	struct list_head_t *pos;
	struct task_t *child;

	/* delete timer */
	if (current_task->sig_tm.list.next)
		timer_event_del(&current_task->sig_tm);

	/* free resources */
	task_exit_signals(current_task);
	task_exit_files(current_task);
	task_exit_fs(current_task);
	task_exit_mmap(current_task->mm);

	/* mark task terminated and reschedule */
	current_task->state = TASK_ZOMBIE;
	current_task->exit_code = status;

	/* wakeup parent */
	task_wakeup_all(&current_task->parent->wait_child_exit);

	/* give children to init */
	list_for_each(pos, &current_task->list) {
		child = list_entry(pos, struct task_t, list);
		if (child->parent == current_task) {
			child->parent = init_task;
			if (child->state == TASK_ZOMBIE)
				task_wakeup_all(&init_task->wait_child_exit);
		}
	}

	/* call scheduler */
	schedule();
}
