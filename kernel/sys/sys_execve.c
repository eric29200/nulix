#include <sys/syscall.h>
#include <proc/sched.h>
#include <proc/elf.h>
#include <x86/tss.h>
#include <mm/mm.h>
#include <stderr.h>
#include <string.h>

/*
 * Copy strings to binary arguments structure.
 */
static void copy_strings(struct binargs_t *bargs, char *const argv[], char *const envp[])
{
	char *str, *p = bargs->buf;
	int i;

	/* copy argv */
	for (i = 0; i < bargs->argc; i++) {
		str = argv[i];
		while (*str)
			*p++ = *str++;
		*p++ = 0;
	}

	/* copy envp */
	for (i = 0; i < bargs->envc; i++) {
		str = envp[i];
		while (*str)
			*p++ = *str++;
		*p++ = 0;
	}
}

/*
 * Init binary arguments.
 */
static int bargs_init(struct binargs_t *bargs, char *const argv[], char *const envp[])
{
	int i;

	/* reset barg */
	memset(bargs, 0, sizeof(struct binargs_t));

	/* get argc */
	for (i = 0; argv && argv[i]; i++)
		bargs->argv_len += strlen(argv[i]) + 1;
	bargs->argc = i;

	/* get envc */
	for (i = 0; envp && envp[i]; i++)
		bargs->envp_len += strlen(envp[i]) + 1;
	bargs->envc = i;

	/* check total size */
	if (bargs->argv_len + bargs->envp_len > PAGE_SIZE)
		return -ENOMEM;

	/* allocate buffer */
	bargs->buf = (char *) kmalloc(bargs->argv_len + bargs->envp_len);
	if (!bargs->buf)
		return -ENOMEM;

	return 0;
}

/*
 * Execve system call.
 */
int sys_execve(const char *path, char *const argv[], char *const envp[])
{
	uint32_t args_str, stack;
	struct binargs_t bargs;
	int ret, i;

	/* init binary arguments */
	ret = bargs_init(&bargs, argv, envp);
	if (ret)
		goto out;

	/* copy argv/envp to binary arguments structure */
	copy_strings(&bargs, argv, envp);

	/* load elf binary */
	ret = elf_load(path);
	if (ret != 0)
		goto out;

	/* put string arguments at the end of the stack */
	args_str = USTACK_START - bargs.argv_len - bargs.envp_len;
	memcpy((void *) args_str, bargs.buf, bargs.argv_len + bargs.envp_len);

	/* set stack base pointer */
	current_task->user_regs.useresp = stack = args_str - (1 + (bargs.argc + 1) + (bargs.envc + 1)) * sizeof(uint32_t);

	/* put argc */
	*((uint32_t *) stack) = bargs.argc;
	stack += 4;

	/* put argv */
	current_task->arg_start = stack;
	for (i = 0; i < bargs.argc; i++) {
		*((uint32_t *) stack) = args_str;
		stack += 4;
		args_str += strlen((char *) args_str) + 1;
	}

	/* finish argv with NULL pointer */
	current_task->arg_end = stack;
	*((uint32_t *) stack) = 0;
	stack += 4;

	/* put envp */
	current_task->env_start = stack;
	for (i = 0; i < bargs.envc; i++) {
		*((uint32_t *) stack) = args_str;
		stack += 4;
		args_str += strlen((char *) args_str) + 1;
	}

	/* finish envp with NULL pointer */
	current_task->env_end = stack;
	*((uint32_t *) stack) = 0;
	stack += 4;

	/* set esp and stack */
	current_task->user_regs.eip = current_task->user_entry;

	ret = 0;
out:
	kfree(bargs.buf);
	return ret;
}
