#ifndef _TASK_H_
#define _TASK_H_

#include <stddef.h>
#include <list.h>

#define STACK_SIZE    0x2000

/*
 * Thread state.
 */
enum thread_state {
  THREAD_READY,
  THREAD_TERMINATED
};

/*
 * Kernel thread structure.
 */
struct thread_t {
  uint32_t tid;
  uint32_t esp;
  uint32_t kernel_stack;
  enum thread_state state;
  struct list_head_t list;
};

/*
 * Registers structure.
 */
struct task_registers_t {
	uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;
	uint32_t eip;
  uint32_t return_address;
  uint32_t parameter1;
  uint32_t parameter2;
  uint32_t parameter3;
};

struct thread_t *create_thread(void (*func)(void));
void destroy_thread(struct thread_t *thread);

#endif
