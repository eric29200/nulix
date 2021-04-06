#ifndef _PROCESS_H_
#define _PROCESS_H_

#include <kernel/system.h>
#include <stddef.h>

/*
 * Process structure.
 */
struct process_t {
  pid_t pid;
  struct registers_t saved_registers;
  uint32_t stack_start;
  uint32_t stack_end;
  struct process_t *next;
};

pid_t start_process(void (*func)(void));
void schedule(struct registers_t *current_registers);

#endif
