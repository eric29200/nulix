#include <proc/sched.h>
#include <proc/binfmt.h>
#include <fs/fs.h>
#include <stderr.h>
#include <stdio.h>
#include <fcntl.h>

/*
 * Load a script.
 */
static int script_load_binary(const char *path, struct binprm *bprm)
{
	char *s, *interp, *interp_name, *i_arg = NULL;
	struct binprm bprm_new;
	struct file *filp;
	char buf[128];
	int fd, ret;
	ssize_t n;
	size_t t;

	/* open file */
	fd = do_open(AT_FDCWD, path, O_RDONLY, 0);
	if (fd < 0)
		return fd;
	filp = current_task->files->filp[fd];

	/* read first characters */
	n = filp->f_op->read(filp, buf, 127, &filp->f_pos);
	buf[127] = 0;

	/* close file */
	sys_close(fd);

	/* check first characters */
	if (n < 3 || buf[0] != '#' || buf[1] != '!') 
		return -ENOEXEC;

	/* keep first line only */
	s = strchr(buf, '\n');
	if (!s)
		s = buf + 127;
	*s = 0;

	/* right trim */
	while (s > buf) {
		s--;
		if (*s == ' ' || *s == '\t')
			*s = 0;
		else
			break;
	}

	/* right trim */
	for (s = buf + 2; *s == ' ' || *s == '\t'; s++);
	if (!s || *s == 0) 
		return -ENOEXEC;

	/* get interpreter path/name */
	interp = interp_name = s;
	for ( ; *s && *s != ' ' && *s != '\t'; s++)
 		if (*s == '/')
			interp_name = s + 1;

	/* trim interpreter */
	while (*s == ' ' || *s == '\t')
		*s++ = 0;

	/* get argument */
	if (*s)
		i_arg = s;

	/* create new binary program */
	bprm_new.argc = 2 + (i_arg ? 1 : 0);
	bprm_new.argv_len = strlen(interp_name) + 1 + (i_arg ? strlen(i_arg) + 1: 0) + strlen(path) + 1;
	bprm_new.envc = bprm->envc;
	bprm_new.envp_len = bprm->envp_len;
	bprm_new.dont_free = 0;

	/* add old arguments (remove first = script name) */
	if (bprm->argc > 1) {
		bprm_new.argc += bprm->argc - 1;
		bprm_new.argv_len += bprm->argv_len - strlen(bprm->buf_args) - 1;
	}

	/* allocate buffer */
	bprm_new.buf_args = bprm_new.p = (char *) kmalloc(bprm_new.argv_len + bprm_new.envp_len);
	if (!bprm_new.buf_args)
		return -ENOMEM;

	/* copy arguments */
	copy_strings(&bprm_new, 1, &interp_name);
	copy_strings(&bprm_new, 1, (char **) &path);
	if(i_arg)
		copy_strings(&bprm_new, 1, &i_arg);

	/* copy old arguments */
	if (bprm->argc > 1) {
		t = strlen(bprm->buf_args) + 1;
		memcpy(bprm_new.p, bprm->buf_args + t, bprm->argv_len - t);
		bprm_new.p += bprm->argv_len - t;
	}

	/* copy environ */
	memcpy(bprm_new.p, bprm->buf_args + bprm->argv_len, bprm->envp_len);

	/* free old binary arguments */
	if (bprm->buf_args) {
		bprm->dont_free = 1;
		kfree(bprm->buf_args);
	}

	/* load binary */
	ret = binary_load(interp, &bprm_new);

	if (!bprm_new.dont_free)
		kfree(bprm_new.buf_args);

	return ret;
}

/*
 * Script binary format.
 */
static struct binfmt script_format = {
	.load_binary		= script_load_binary,
};

/*
 * Init Script binary format.
 */
int init_script_binfmt()
{
	return register_binfmt(&script_format);
}