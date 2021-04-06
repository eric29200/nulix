#include <kernel/process.h>
#include <kernel/mm.h>
#include <stderr.h>
#include <string.h>

/* processes list */
struct process_t *processes_list = NULL;

/* current pid */
static volatile int current_pid = 1;

/* current process */
struct process_t *current_process = NULL;

/* kernel stack defined in boot.s */
extern uint32_t kernel_stack;

/*
 * Start a process.
 */
pid_t start_process(void (*func)(void))
{
  struct process_t *process;

  /* allocate new process */
  process = (struct process_t *) kmalloc(sizeof(struct process_t));
  if (!process)
    return ENOMEM;

  /* set process */
  process->pid = current_pid++;

  /* allocate a stack (1 MB) */
  process->stack_start = (uint32_t) kmalloc(1024 * 1024);
  process->stack_end = process->stack_start + 1024 *1024;

  /* set registers */
  memset(&process->saved_registers, 0, sizeof(struct registers_t));
  process->saved_registers.ds = KERNEL_DATA_SEGMENT | 0;
  process->saved_registers.ss = KERNEL_DATA_SEGMENT | 0;
  process->saved_registers.cs = KERNEL_CODE_SEGMENT | 0;
  process->saved_registers.eflags = 0x206;
  process->saved_registers.eip = (uint32_t) func;
  process->saved_registers.useresp = process->stack_end;

  /* add to the processes list */
  process->next = processes_list;
  processes_list = process;

  return process->pid;
}

/*
 * Schedule.
 */
void schedule(struct registers_t *current_registers)
{
  /* take next process */
  if (current_process)
    current_process = current_process->next;

  /* or first one if at the end of the list*/
  if (current_process == NULL)
    current_process = processes_list;

  /* switch to current process */
  if (current_process)
    memcpy(current_registers, &current_process->saved_registers, sizeof(struct registers_t));
}
