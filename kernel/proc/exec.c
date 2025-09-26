#include <proc/sched.h>
#include <proc/binfmt.h>
#include <proc/elf.h>
#include <stdio.h>
#include <stderr.h>

/* binary formats */
static LIST_HEAD(formats);

/*
 * Copy strings to binary program structure.
 */
void copy_strings(struct binprm *bprm, int argc, char **argv)
{
	char *str, *p = bprm->p;
	int i;

	/* copy argv */
	for (i = 0; i < argc; i++) {
		str = argv[i];
		while (*str)
			*p++ = *str++;
		*p++ = 0;
	}

	/* update program position */
	bprm->p = p;
}

/*
 * Load a binary file.
 */
int search_binary_handler(struct binprm *bprm)
{
	struct list_head *pos;
	struct binfmt *fmt;
	int ret;

	/* find binary format */
	list_for_each(pos, &formats) {
		fmt = list_entry(pos, struct binfmt, list);

		/* load binary */
		ret = fmt->load_binary(bprm);
		if (ret == 0)
			return ret;
	}

	return ret;
}

/*
 * Execve system call.
 */
int sys_execve(const char *path, char *const argv[], char *const envp[])
{
	struct binprm bprm;
	size_t i;
	int ret;

	/* reset barg */
	memset(&bprm, 0, sizeof(struct binprm));
	bprm.filename = path;

	/* get argc */
	for (i = 0; argv && argv[i]; i++)
		bprm.argv_len += strlen(argv[i]) + 1;
	bprm.argc = i;

	/* get envc */
	for (i = 0; envp && envp[i]; i++)
		bprm.envp_len += strlen(envp[i]) + 1;
	bprm.envc = i;

	/* allocate buffer */
	bprm.buf_args = bprm.p = (char *) kmalloc(bprm.argv_len + bprm.envp_len);
	if (!bprm.buf_args)
		return -ENOMEM;

	/* copy argv/envp to binary program structure */
	copy_strings(&bprm, bprm.argc, (char **) argv);
	copy_strings(&bprm, bprm.envc, (char **) envp);

	/* load binary */
	ret = search_binary_handler(&bprm);

	/* free buffer */
	if (!bprm.dont_free)
		kfree(bprm.buf_args);

	return ret;
}

/*
 * Register a binary format.
 */
int register_binfmt(struct binfmt *fmt)
{
	struct list_head *pos;
	struct binfmt *tmp;

	/* check binary format */
	if (!fmt)
		return -EINVAL;

	/* check if this binary format is already registered */
	list_for_each(pos, &formats) {
		tmp = list_entry(pos, struct binfmt, list);
		if (fmt == tmp)
			return -EBUSY;
	}

	/* register binary format */
	list_add(&fmt->list, &formats);

	return 0;
}

/*
 * Init binary formats.
 */
void init_binfmt()
{
	init_elf_binfmt();
	init_script_binfmt();
}