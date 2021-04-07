#ifndef _TASK_H_
#define _TASK_H_

#include <stddef.h>

#define STACK_SIZE    (1024 * 1024)

/*
 * Kernel thread structure.
 */
struct thread_t {
  uint32_t tid;
  uint32_t esp;
  uint32_t kernel_stack;
  struct thread_t *next;
};

/*
 * Registers structure.
 */
struct task_registers_t {
	uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;
	uint32_t eip;
};

int create_thread(void (*func)(void));
void schedule();

#endif
