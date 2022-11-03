#include <sys/syscall.h>
#include <stderr.h>

/*
 * Set session id system call.
 */
int sys_setsid()
{
	current_task->leader = 1;
	current_task->pgrp = current_task->pid;
	current_task->session = current_task->pid;
	current_task->tty = -1;

	return current_task->session;
}
