#include <x86/tls.h>
#include <x86/gdt.h>
#include <proc/sched.h>
#include <stderr.h>
#include <stdio.h>

/*
 * Load TLS.
 */
void load_tls()
{
	gdt_write_entry(GDT_ENTRY_TLS, &current_task->thread.tls);
}

/*
 * Set TLS descriptor.
 */
static void set_tls_desc(struct task *task, struct user_desc *u_info)
{
	struct desc_struct *desc = &task->thread.tls;

	if (LDT_empty(u_info) || LDT_zero(u_info))
		memset(desc, 0, sizeof(*desc));
	else
		fill_ldt(desc, u_info);
}

/*
 * Fill user descriptor.
 */
static void fill_user_desc(struct user_desc *info, int idx, struct desc_struct *desc)
{
	memset(info, 0, sizeof(*info));
	info->entry_number = idx;
	info->base_addr = get_desc_base(desc);
	info->limit = get_desc_limit(desc);
	info->seg_32bit = desc->d;
	info->contents = desc->type >> 2;
	info->read_exec_only = !(desc->type & 2);
	info->limit_in_pages = desc->g;
	info->seg_not_present = !desc->p;
	info->useable = desc->avl;
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
	fill_user_desc(u_info, 0, &current_task->thread.tls);

	return 0;
}

/*
 * Set thread area system call.
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
	set_tls_desc(current_task, u_info);

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
