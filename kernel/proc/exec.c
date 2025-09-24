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
 * Init binary program.
 */
static int bprm_init(struct binprm *bprm, char *const argv[], char *const envp[])
{
	int i;

	/* reset barg */
	memset(bprm, 0, sizeof(struct binprm));

	/* get argc */
	for (i = 0; argv && argv[i]; i++)
		bprm->argv_len += strlen(argv[i]) + 1;
	bprm->argc = i;

	/* get envc */
	for (i = 0; envp && envp[i]; i++)
		bprm->envp_len += strlen(envp[i]) + 1;
	bprm->envc = i;

	/* allocate buffer */
	bprm->buf = bprm->p = (char *) kmalloc(bprm->argv_len + bprm->envp_len);
	if (!bprm->buf)
		return -ENOMEM;

	return 0;
}

/*
 * Load a binary file.
 */
int binary_load(const char *path, struct binprm *bprm)
{
	struct list_head *pos;
	struct binfmt *fmt;
	int ret;

	/* find binary format */
	list_for_each(pos, &formats) {
		fmt = list_entry(pos, struct binfmt, list);

		/* load binary */
		ret = fmt->load_binary(path, bprm);
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
	int ret;

	/* init binary program */
	ret = bprm_init(&bprm, argv, envp);
	if (ret)
		goto out;

	/* copy argv/envp to binary program structure */
	copy_strings(&bprm, bprm.argc, (char **) argv);
	copy_strings(&bprm, bprm.envc, (char **) envp);

	/* load binary */
	ret = binary_load(path, &bprm);
out:
	if (!bprm.dont_free)
		kfree(bprm.buf);
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