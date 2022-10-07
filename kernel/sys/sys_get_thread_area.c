#include <sys/syscall.h>
#include <stderr.h>

/*
 * Get thread area system call.
 */
int sys_get_thread_area(struct user_desc_t *u_info)
{
	return do_get_thread_area(u_info);
}
