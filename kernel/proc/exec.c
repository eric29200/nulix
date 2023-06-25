#include <proc/sched.h>
#include <proc/elf.h>
#include <stderr.h>

/*
 * Copy strings to binary arguments structure.
 */
void copy_strings(struct binargs_t *bargs, int argc, char **argv)
{
	char *str, *p = bargs->p;
	int i;

	/* copy argv */
	for (i = 0; i < argc; i++) {
		str = argv[i];
		while (*str)
			*p++ = *str++;
		*p++ = 0;
	}

	/* update binargs position */
	bargs->p = p;
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
	bargs->buf = bargs->p = (char *) kmalloc(bargs->argv_len + bargs->envp_len);
	if (!bargs->buf)
		return -ENOMEM;

	return 0;
}

/*
 * Load a binary file.
 */
int binary_load(const char *path, struct binargs_t *bargs)
{
	int ret;

	/* try to load elf binary or script */
	ret = elf_load(path, bargs);
	if (ret)
		ret = script_load(path, bargs);

	return ret;
}

/*
 * Execve system call.
 */
int sys_execve(const char *path, char *const argv[], char *const envp[])
{
	struct binargs_t bargs;
	int ret;

	/* init binary arguments */
	ret = bargs_init(&bargs, argv, envp);
	if (ret)
		goto out;

	/* copy argv/envp to binary arguments structure */
	copy_strings(&bargs, bargs.argc, (char **) argv);
	copy_strings(&bargs, bargs.envc, (char **) envp);

	/* load binary */
	ret = binary_load(path, &bargs);
out:
	if (!bargs.dont_free)
		kfree(bargs.buf);
	return ret;
}