#ifndef _TLS_H_
#define _TLS_H_

#include <stddef.h>

/*
 * User TLS.
 */
struct user_desc {
	uint32_t	entry_number;
	uint32_t	base_addr;
	uint32_t	limit;
	uint32_t	seg_32bit:1;
	uint32_t	contents:2;
	uint32_t	read_exec_only:1;
	uint32_t	limit_in_pages:1;
	uint32_t	seg_not_present:1;
	uint32_t	useable:1;
};


void load_tls();
int sys_get_thread_area(struct user_desc *u_info);
int sys_set_thread_area(struct user_desc *u_info);
pid_t sys_set_tid_address(int *tidptr);

#endif
