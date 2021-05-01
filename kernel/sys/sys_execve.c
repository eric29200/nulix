#include <sys/syscall.h>
#include <proc/sched.h>
#include <proc/elf.h>
#include <x86/tss.h>
#include <mm/mm.h>
#include <stderr.h>
#include <string.h>

/* switch to user mode (defined in x86/scheduler.s) */
extern void enter_user_mode(uint32_t esp, uint32_t eip, uint32_t return_address);

/*
 * Execve system call.
 */
int sys_execve(const char *path, char *const argv[], char *const envp[])
{
  char **kernel_argv = NULL, **kernel_envp = NULL;
  char **user_argv = NULL, **user_envp = NULL;
  int ret, i, argv_len, envp_len, len;
  uint32_t stack;

  /* get argv and envp size */
  argv_len = array_nb_pointers((void *) argv);
  envp_len = array_nb_pointers((void *) envp);

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
    goto err;

  /* copy back argv to user address space */
  user_argv = (char **) sys_sbrk(sizeof(char *) * argv_len);
  memset(user_argv, 0, sizeof(char *) * argv_len);
  if (kernel_argv) {
    for (i = 0; i < argv_len; i++) {
      if (kernel_argv[i]) {
        len = strlen(kernel_argv[i]);
        user_argv[i] = (char *) sys_sbrk(len + 1);
        memset(user_argv[i], 0, len + 1);
        memcpy(user_argv[i], kernel_argv[i], len);
        kfree(kernel_argv[i]);
      }
    }
    kfree(kernel_argv);
  }

  /* copy back envp to user address space */
  user_envp = (char **) sys_sbrk(sizeof(char *) * envp_len);
  memset(user_envp, 0, sizeof(char *) * envp_len);
  if (kernel_envp) {
    for (i = 0; i < envp_len; i++) {
      if (kernel_envp[i]) {
        len = strlen(kernel_envp[i]);
        user_envp[i] = (char *) sys_sbrk(len + 1);
        memset(user_envp[i], 0, len + 1);
        memcpy(user_envp[i], kernel_envp[i], len);
        kfree(kernel_envp[i]);
      }
    }
    kfree(kernel_envp);
  }

  /* put argc, argv and envp in kernel stack */
  stack = current_task->user_stack;
  stack -= 4;
  *((uint32_t *) stack) = (uint32_t) user_envp;
  stack -= 4;
  *((uint32_t *) stack) = (uint32_t) user_argv;
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
