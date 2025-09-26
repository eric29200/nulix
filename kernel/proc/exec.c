#include <proc/sched.h>
#include <proc/binfmt.h>
#include <proc/elf.h>
#include <stdio.h>
#include <stderr.h>
#include <fcntl.h>

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
 * Open a dentry.
 */
int open_dentry(struct dentry *dentry, mode_t mode)
{
	struct inode *inode = dentry->d_inode;
	struct file *filp;
	int fd, ret;

	/* check inode */
	ret = -EINVAL;
	if (!inode->i_op || !inode->i_op->fops)
		goto out;

	/* get a file slot */
	fd = get_unused_fd();
	if (fd < 0)
		return fd;

	/* get a file */
	ret = -ENFILE;
	filp = get_empty_filp();
	if (!filp)
		goto out;

	/* set file */
	filp->f_flags = mode;
	filp->f_mode = (mode + 1) & O_ACCMODE;
	filp->f_dentry = dget(dentry);
	filp->f_pos = 0;
	filp->f_op = inode->i_op->fops;

	/* open file */
	if (filp->f_op->open) {
		ret = filp->f_op->open(inode, filp);
		if (ret)
			goto out_fput;
	}

	/* install file */
	current_task->files->filp[fd] = filp;
	return fd;
out_fput:
	fput(filp);
out:
	return ret;
}

/*
 * Prepare a binary program.
 */
int prepare_binprm(struct binprm *bprm)
{
	struct inode *inode = bprm->dentry->d_inode;
	int mode = inode->i_mode;
	int ret;

	/* muse be a regular file */
	if (!S_ISREG(mode))
		return -EACCES;

	/* with at least one execute bit set */
	if (!(mode & 0111))
		return -EACCES;

	/* filesystem must not be mounted noexec */
	if (IS_NOEXEC(inode))
		return -EACCES;
	if (!inode->i_sb)
		return -EACCES;

	/* check permissions */
	ret = permission(inode, MAY_EXEC);
	if (ret)
		return ret;

	return 0;
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

		/* handle success */
		if (ret >= 0)
			return ret;

		/* handle fatal error */
		if (ret != -ENOEXEC || !bprm->dentry)
			return ret;
	}

	return ret;
}

/*
 * Execve system call.
 */
int sys_execve(const char *path, char *const argv[], char *const envp[])
{
	struct dentry *dentry;
	struct binprm bprm;
	size_t i;
	int ret;

	/* resolve path */
	dentry = open_namei(AT_FDCWD, path, 0, 0);
	if (IS_ERR(dentry))
		return PTR_ERR(dentry);

	/* reset barg */
	memset(&bprm, 0, sizeof(struct binprm));
	bprm.filename = path;
	bprm.dentry = dentry;

	/* prepare binary program */
	ret = prepare_binprm(&bprm);
	if (ret < 0)
		goto out;

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

	/* handle success */
	if (ret >= 0)
		return ret;
out:
	/* on error, clean binary program */
	if (bprm.dentry)
		dput(bprm.dentry);
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