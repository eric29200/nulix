#ifndef _TASK_H_
#define _TASK_H_

#include <proc/elf.h>
#include <mm/paging.h>
#include <fs/fs.h>
#include <stddef.h>
#include <list.h>

#define STACK_SIZE        0x2000

#define TASK_RUNNING      1
#define TASK_SLEEPING     2
#define TASK_ZOMBIE       3

/*
 * Kernel task structure.
 */
struct task_t {
  pid_t pid;
  uint32_t esp;
  uint32_t kernel_stack;
  uint8_t state;
  char *path;
  dev_t tty;
  uint32_t user_entry;
  uint32_t user_stack;
  uint32_t user_stack_size;
  uint32_t start_brk;
  uint32_t end_brk;
  int exit_code;
  struct registers_t user_regs;
  struct page_directory_t *pgd;
  struct file_t *filp[NR_OPEN];
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

struct task_t *create_kernel_task(void (*func)(void));
void destroy_task(struct task_t *task);
int spawn_init();

void sys_exit(int status);
pid_t sys_fork();
int sys_execve(const char *path, char *const argv[], char *const envp[]);
void *sys_sbrk(size_t n);

#endif
