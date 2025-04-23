#ifndef _LDT_H_
#define _LDT_H_

#include <x86/segment.h>
#include <x86/descriptor.h>
#include <proc/task.h>

#define LDT_ENTRIES		8192
#define LDT_ENTRY_SIZE		8

int sys_modify_ldt(int func, void *ptr, uint32_t bytecount);
void switch_ldt(struct task *prev, struct task *next);
int clone_ldt(struct mm_struct *src, struct mm_struct *dst);

#endif
