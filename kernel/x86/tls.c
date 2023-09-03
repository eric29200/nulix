#include <x86/tls.h>
#include <proc/sched.h>
#include <stderr.h>

/*
 * Load TLS.
 */
void load_tls()
{
	__asm__ volatile("movl %0, %%gs:0" :: "r" (current_task->tls.base_addr));
}

/*
 * Get thread area.
 */
int sys_get_thread_area(struct user_desc *u_info)
{
	int idx = u_info->entry_number;

	/* check entry number */
	if (idx != 0)
		return -EINVAL;

	/* copy TLS */
	memcpy(u_info, &current_task->tls, sizeof(struct user_desc));

	return 0;
}

/*
 * Set thread area.
 */
int sys_set_thread_area(struct user_desc *u_info)
{
	int idx = u_info->entry_number;

	/* find an entry */
	if (idx == -1)
		idx = 0;

	/* only first entry allowed */
	if (idx != 0)
		return -EINVAL;

	/* set TLS */
	u_info->entry_number = idx;
	memcpy(&current_task->tls, u_info, sizeof(struct user_desc));

	/* load TLS */
	load_tls();

	return 0;
}

/*
 * Set pointer to thread id system call.
 */
pid_t sys_set_tid_address(int *tidptr)
{
	UNUSED(tidptr);
	return current_task->pid;
}
