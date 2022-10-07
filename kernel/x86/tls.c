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
int do_get_thread_area(struct user_desc_t *u_info)
{
	int idx = u_info->entry_number;

	/* check entry number */
	if (idx != 0)
		return -EINVAL;

	/* copy TLS */
	memcpy(u_info, &current_task->tls, sizeof(struct user_desc_t));

	return 0;
}

/*
 * Set thread area.
 */
int do_set_thread_area(struct user_desc_t *u_info)
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
	memcpy(&current_task->tls, u_info, sizeof(struct user_desc_t));

	/* load TLS */
	load_tls();

	return 0;
}
