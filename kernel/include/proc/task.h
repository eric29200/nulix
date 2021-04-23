#ifndef _TASK_H_
#define _TASK_H_

#include <proc/elf.h>
#include <mm/paging.h>
#include <fs/fs.h>
#include <stddef.h>
#include <list.h>

#define STACK_SIZE        0x2000

#define TASK_NEW          1
#define TASK_READY        2
#define TASK_WAITING      3
#define TASK_TERMINATED   4

/*
 * Kernel task structure.
 */
struct task_t {
  pid_t pid;
  uint32_t esp;
  uint32_t kernel_stack;
  uint8_t state;
  uint32_t expires;
  char *path;
  struct elf_layout_t *elf_layout;
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

struct task_t *create_kernel_task(void (*func)(void *), void *arg);
struct task_t *create_user_elf_task(const char *path);
void destroy_task(struct task_t *task);

#endif
