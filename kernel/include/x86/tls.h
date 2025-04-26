#ifndef _TLS_H_
#define _TLS_H_

#include <x86/descriptor.h>
#include <proc/task.h>

void load_tls();

int sys_get_thread_area(struct user_desc *u_info);
int sys_set_thread_area(struct user_desc *u_info);
pid_t sys_set_tid_address(int *tidptr);

#endif
