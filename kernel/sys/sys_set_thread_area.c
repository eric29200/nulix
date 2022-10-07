#include <sys/syscall.h>
#include <stderr.h>

/*
 * Set thread area system call.
 */
int sys_set_thread_area(struct user_desc_t *u_info)
{
	return do_set_thread_area(u_info);
}
