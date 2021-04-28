#include <x86/interrupt.h>
#include <x86/tss.h>
#include <mm/mm.h>
#include <proc/task.h>
#include <proc/sched.h>
#include <proc/elf.h>
#include <string.h>
#include <stderr.h>

/* switch to user mode (defined in x86/scheduler.s) */
extern void enter_user_mode(uint32_t esp, uint32_t eip, uint32_t return_address);
extern void return_user_mode(struct registers_t *regs);

/*
 * Kernel fork trampoline.
 */
static void task_user_entry(struct task_t *task)
{
  /* return to user mode */
  tss_set_stack(0x10, task->kernel_stack);
  return_user_mode(&task->user_regs);
}

/*
 * Kernel ELF task trampoline (used to end tasks properly).
 */
static void task_elf_entry(struct task_t *task, char *path)
{
  /* load elf header */
  task->path = path;
  if (elf_load(path) == 0)
    enter_user_mode(task->user_stack, task->user_entry, TASK_RETURN_ADDRESS);
}

/*
 * Create and init a task.
 */
static struct task_t *create_task()
{
  struct task_t *task;
  void *stack;
  int i;

  /* create task */
  task = (struct task_t *) kmalloc(sizeof(struct task_t));
  if (!task)
    return NULL;

  /* set tid */
  task->pid = get_next_pid();
  task->state = TASK_NEW;
  task->expires = 0;
  task->path = NULL;
  task->user_stack_size = 0;
  INIT_LIST_HEAD(&task->list);

  /* init open files */
  for (i = 0; i < NR_OPEN; i++)
    task->filp[i] = NULL;

  /* allocate stack */
  stack = (void *) kmalloc(STACK_SIZE);
  if (!stack) {
    kfree(task);
    return NULL;
  }

  /* set stack */
  memset(stack, 0, STACK_SIZE);
  task->kernel_stack = (uint32_t) stack + STACK_SIZE;
  task->esp = task->kernel_stack - sizeof(struct task_registers_t);

  return task;
}

/*
 * Create a task.
 */
struct task_t *create_kernel_task(void (*func)(void))
{
  struct task_registers_t *regs;
  struct task_t *task;

  /* create task */
  task = create_task();
  if (!task)
    return NULL;

  /* use kernel page directory */
  task->pgd = kernel_pgd;

  /* set registers */
  regs = (struct task_registers_t *) task->esp;
  memset(regs, 0, sizeof(struct task_registers_t));

  /* set eip to function */
  regs->return_address = TASK_RETURN_ADDRESS;
  regs->eip = (uint32_t) func;
  regs->eax = 0;
  regs->ecx = 0;
  regs->edx = 0;
  regs->ebx = 0;
  regs->esp = 0;
  regs->ebp = 0;
  regs->esi = 0;
  regs->edi = 0;

  return task;
}

/*
 * Fork a task.
 */
static struct task_t *fork_task(struct task_t *parent)
{
  struct task_registers_t *regs;
  struct task_t *task;
  int i;

  /* create task */
  task = create_task();
  if (!task)
    return NULL;

  /* duplicate page directory */
  task->pgd = clone_page_directory(parent->pgd);

  /* copy open files */
  for (i = 0; i < NR_OPEN; i++) {
    task->filp[i] = parent->filp[i];
    if (task->filp[i])
      task->filp[i]->f_ref++;
  }

  /* set user stack to parent */
  task->user_stack = parent->user_stack;
  task->user_stack_size = parent->user_stack_size;

  memcpy(&task->user_regs, &parent->user_regs, sizeof(struct registers_t));
  task->user_regs.eax = 0;

  /* set registers */
  regs = (struct task_registers_t *) task->esp;
  memset(regs, 0, sizeof(struct task_registers_t));

  /* set eip to function */
  regs->parameter1 = (uint32_t) task;
  regs->return_address = TASK_RETURN_ADDRESS;
  regs->eip = (uint32_t) task_user_entry;
  regs->eax = 0;
  regs->ecx = 0;
  regs->edx = 0;
  regs->ebx = 0;
  regs->esp = 0;
  regs->ebp = 0;
  regs->esi = 0;
  regs->edi = 0;

  return task;
}

/*
 * Fork system call.
 */
pid_t sys_fork()
{
  struct task_t *child;
  int ret;

  /* create child */
  child = fork_task(current_task);
  if (!child)
    return -ENOMEM;

  /* run child */
  ret = run_task(child);
  if (ret != 0) {
    destroy_task(child);
    return ret;
  }

  /* return child pid */
  return child->pid;
}

/*
 * Create an ELF task.
 */
struct task_t *create_user_elf_task(const char *path)
{
  struct task_registers_t *regs;
  struct task_t *task;

  /* create task */
  task = create_task();
  if (!task)
    return NULL;

  /* clone page directory */
  task->pgd = clone_page_directory(current_task->pgd);

  /* set registers */
  regs = (struct task_registers_t *) task->esp;
  memset(regs, 0, sizeof(struct task_registers_t));

  /* set eip */
  regs->parameter1 = (uint32_t) task;
  regs->parameter2 = (uint32_t) strdup(path);
  regs->return_address = TASK_RETURN_ADDRESS;
  regs->eip = (uint32_t) task_elf_entry;
  regs->eax = 0;
  regs->ecx = 0;
  regs->edx = 0;
  regs->ebx = 0;
  regs->esp = 0;
  regs->ebp = 0;
  regs->esi = 0;
  regs->edi = 0;

  return task;
}

/*
 * Execve system call.
 */
int sys_execve(const char *path, const char *argv[], char *envp[])
{
  char **kernel_argv = NULL, **kernel_envp = NULL;
  int ret, i, argv_len, envp_len;
  uint32_t stack;

  /* get argv and envp size */
  argv_len = array_nb_pointers(argv);
  envp_len = array_nb_pointers(envp);

  /* copy argv in kernel memory */
  if (argv_len == 0) {
    kernel_argv = NULL;
  } else {
    kernel_argv = (char **) kmalloc(sizeof(char *) * argv_len);
    if (!kernel_argv) {
      ret = -ENOMEM;
      goto err;
    }

    for (i = 0; i < argv_len; i++)
      kernel_argv[i] = NULL;

    for (i = 0; i < argv_len; i++) {
      if (argv[i]) {
        kernel_argv[i] = strdup(argv[i]);
        if (!kernel_argv[i]) {
          ret = -ENOMEM;
          goto err;
        }
      }
    }
  }

  /* copy envp in kernel memory */
  if (envp_len == 0) {
    kernel_envp = NULL;
  } else {
    kernel_envp = (char **) kmalloc(sizeof(char *) * envp_len);
    if (!kernel_envp) {
      ret = -ENOMEM;
      goto err;
    }

    for (i = 0; i < envp_len; i++)
      kernel_envp[i] = NULL;


    for (i = 0; i < envp_len; i++) {
      if (envp[i]) {
        kernel_envp[i] = strdup(envp[i]);
        if (!kernel_envp[i]) {
          ret = -ENOMEM;
          goto err;
        }
      }
    }
  }

  /* load elf binary */
  ret = elf_load(path);
  if (ret != 0)
    return ret;

  /* set path */
  current_task->path = strdup(path);

  /* put argc, argv and envp in kernel stack */
  stack = current_task->user_stack;
  stack -= 4;
  *((uint32_t *) stack) = (uint32_t) kernel_envp;
  stack -= 4;
  *((uint32_t *) stack) = (uint32_t) kernel_argv;
  stack -= 4;
  *((uint32_t *) stack) = argv_len;

  /* go to user mode */
  tss_set_stack(0x10, current_task->kernel_stack);
  enter_user_mode(stack, current_task->user_entry, TASK_RETURN_ADDRESS);

  return 0;
err:
  if (kernel_argv) {
    for (i = 0; i < argv_len; i++)
      if (kernel_argv[i])
        kfree(kernel_argv[i]);

    kfree(kernel_argv);
  }

  if (kernel_envp) {
    for (i = 0; i < envp_len; i++)
      if (kernel_envp[i])
        kfree(kernel_envp[i]);

    kfree(kernel_envp);
  }
  return ret;
}

/*
 * Destroy a task.
 */
void destroy_task(struct task_t *task)
{
  char *user_stack;

  if (!task)
    return;

  /* free task path */
  if (task->path)
    kfree(task->path);

  /* free user stack */
  if (task->user_stack_size > 0) {
    user_stack = (char *) task->user_stack - task->user_stack_size;
    kfree(user_stack);
  }

  /* free kernel stack */
  kfree((void *) (task->kernel_stack - STACK_SIZE));

  /* free page directory */
  if (task->pgd != kernel_pgd)
    free_page_directory(task->pgd);

  /* free task */
  kfree(task);
}
