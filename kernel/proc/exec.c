#include <proc/sched.h>
#include <proc/binfmt.h>
#include <proc/ptrace.h>
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
 * Read a dentry.
 */
int read_exec(struct dentry *dentry, off_t offset, char *addr, size_t count)
{
	struct inode *inode = dentry->d_inode;
	int ret = -ENOEXEC;
	struct file file;

	/* check inode */
	if (!inode->i_op || !inode->i_op->fops)
		goto out;

	/* set a private file */
	if (init_private_file(&file, dentry, 1))
		goto out;
	if (!file.f_op->read)
		goto out_release;

	/* seek to position */
	if (file.f_op->llseek) {
		if (file.f_op->llseek(&file, offset, 0) != offset)
 			goto out_release;
	} else {
		file.f_pos = offset;
	}

	/* read */
	ret = file.f_op->read(&file, addr, count, &file.f_pos);
out_release:
	if (file.f_op->release)
		file.f_op->release(inode, &file);
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

	/* uid/gid */
	bprm->e_uid = current_task->euid;
	bprm->e_gid = current_task->egid;
	bprm->priv_change = 0;

	/* set uid ? */
	if (mode & S_ISUID) {
		bprm->e_uid = inode->i_uid;
		if (bprm->e_uid != current_task->euid)
			bprm->priv_change = 1;
	}

	/* set gid ? */
	if ((mode & (S_ISGID | S_IXGRP)) == (S_ISGID | S_IXGRP)) {
		bprm->e_gid = inode->i_gid;
		if (!task_in_group(current_task, bprm->e_gid))
			bprm->priv_change = 1;
	}

	/* check permissions */
	if (bprm->priv_change) {
		current_task->dumpable = 0;

		if (IS_NOSUID(inode)
			|| (current_task->ptrace & PT_PTRACED)
			|| current_task->fs->count > 1
			|| current_task->sig->count > 1
			|| current_task->files->count > 1)
			return -EPERM;
	}

	/* read header */
	memset(bprm->buf, 0, sizeof(bprm->buf));
	return read_exec(bprm->dentry, 0, bprm->buf, 128);
}

/*
 * Clear current executable (clear memory, signals and files).
 */
int flush_old_exec(struct binprm *bprm)
{
	struct signal_struct *sig_new = NULL, *sig_old;
	struct mm_struct *mm_new;
	int fd, i, ret;

	/* make sig private */
	sig_old = current_task->sig;
	if (current_task->sig->count > 1) {
		/* allocate a private sig */
		sig_new = (struct signal_struct *) kmalloc(sizeof(struct signal_struct));
		if (!sig_new) {
			ret = -ENOMEM;
			goto err_sig;
		}

		/* copy actions */
		sig_new->count = 1;
		memcpy(sig_new->action, current_task->sig->action, sizeof(sig_new->action));
		current_task->sig = sig_new;
	}


	/* clear all memory regions */
	if (current_task->mm->count == 1) {
		task_release_mmap(current_task);
		task_exit_mmap(current_task->mm);
	} else {
		/* duplicate mm struct */
		mm_new = task_dup_mm(current_task->mm);
		if (!mm_new) {
			ret = -ENOMEM;
			goto err_mm;
		}

		/* decrement old mm count and set new mm struct */
		current_task->mm->count--;
		current_task->mm = mm_new;

		/* switch to new page directory */
		switch_pgd(current_task->mm->pgd);

		/* release mapping */
		task_release_mmap(current_task);
	}

	/* release old signals */
	if (current_task->sig != sig_old)
		sig_old->count--;

	/* clear signal handlers */
	for (i = 0; i < NSIGS; i++) {
		if (current_task->sig->action[i].sa_handler != SIG_IGN)
			current_task->sig->action[i].sa_handler = SIG_DFL;

		current_task->sig->action[i].sa_flags = 0;
		sigemptyset(&current_task->sig->action[i].sa_mask);
	}

	/* close files marked close on exec */
	for (fd = 0; fd < NR_OPEN; fd++) {
		if (FD_ISSET(fd, &current_task->files->close_on_exec)) {
			sys_close(fd);
			FD_CLR(fd, &current_task->files->close_on_exec);
		}
	}

	/* dumpable ? */
	bprm->dumpable = 0;
	if (current_task->euid == current_task->uid && current_task->egid == current_task->gid)
		bprm->dumpable = !bprm->priv_change;
	else
		current_task->dumpable = 0;

	if (bprm->e_uid != current_task->euid || bprm->e_gid != current_task->egid || permission(bprm->dentry->d_inode, MAY_READ)) {
		bprm->dumpable = 0;
		current_task->dumpable = 0;
	}

	return 0;
err_mm:
	if (sig_new)
		kfree(sig_new);
err_sig:
	current_task->sig = sig_old;
	return ret;
}

/*
 * Compute credentials.
 */
void compute_creds(struct binprm *bprm)
{
	current_task->suid = bprm->e_uid;
	current_task->euid = bprm->e_uid;
	current_task->fsuid = bprm->e_uid;
	current_task->sgid = bprm->e_gid;
	current_task->egid = bprm->e_gid;
	current_task->fsgid = bprm->e_gid;

	if (current_task->euid != current_task->uid || current_task->egid != current_task->gid) {
		bprm->dumpable = 0;
		current_task->dumpable = 0;
	}
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
	int ret, was_dumpable;
	struct binprm bprm;
	size_t i;

	/* resolve path */
	dentry = open_namei(AT_FDCWD, path, 0, 0);
	if (IS_ERR(dentry))
		return PTR_ERR(dentry);

	/* reset barg */
	memset(&bprm, 0, sizeof(struct binprm));
	bprm.filename = path;
	bprm.dentry = dentry;
	was_dumpable = current_task->dumpable;
	current_task->dumpable = 0;

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
	if (ret >= 0) {
		current_task->dumpable = bprm.dumpable;
		return ret;
	}
out:
	/* on error, clean binary program */
	if (bprm.dentry)
		dput(bprm.dentry);
	kfree(bprm.buf_args);
	current_task->dumpable = was_dumpable;
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