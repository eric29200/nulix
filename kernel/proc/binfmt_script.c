#include <proc/sched.h>
#include <proc/binfmt.h>
#include <fs/fs.h>
#include <stderr.h>
#include <stdio.h>
#include <fcntl.h>

/*
 * Load a script.
 */
static int script_load_binary(struct binprm *bprm)
{
	char *s, *interp, *interp_name, *i_arg = NULL;
	struct binprm bprm_new;
	struct dentry *dentry;
	size_t t;
	int ret;

	/* check first characters */
	if (bprm->buf[0] != '#' || bprm->buf[1] != '!' || bprm->sh_bang) 
		return -ENOEXEC;

	/* release dentry */
	bprm->sh_bang++;
	dput(bprm->dentry);
	bprm->dentry = NULL;

	/* keep first line only */
	s = strchr(bprm->buf, '\n');
	if (!s)
		s = bprm->buf + 127;
	*s = 0;

	/* right trim */
	while (s > bprm->buf) {
		s--;
		if (*s == ' ' || *s == '\t')
			*s = 0;
		else
			break;
	}

	/* right trim */
	for (s = bprm->buf + 2; *s == ' ' || *s == '\t'; s++);
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

	/* resolve interpreter path */
	dentry = open_namei(AT_FDCWD, interp, 0, 0);
	if (IS_ERR(dentry))
		return PTR_ERR(dentry);

	/* create new binary program */
	memset(&bprm_new, 0, sizeof(struct binprm));
	bprm_new.filename = bprm->filename;
	bprm_new.argc = 2 + (i_arg ? 1 : 0);
	bprm_new.argv_len = strlen(interp_name) + 1 + (i_arg ? strlen(i_arg) + 1: 0) + strlen(bprm->filename) + 1;
	bprm_new.envc = bprm->envc;
	bprm_new.envp_len = bprm->envp_len;

	/* add old arguments (remove first = script name) */
	if (bprm->argc > 1) {
		bprm_new.argc += bprm->argc - 1;
		bprm_new.argv_len += bprm->argv_len - strlen(bprm->buf_args) - 1;
	}

	/* allocate buffer */
	bprm_new.buf_args = bprm_new.p = (char *) kmalloc(bprm_new.argv_len + bprm_new.envp_len);
	if (!bprm_new.buf_args) {
		dput(dentry);
		return -ENOMEM;
	}

	/* copy arguments */
	copy_strings(&bprm_new, 1, &interp_name);
	copy_strings(&bprm_new, 1, (char **) &bprm->filename);
	if (i_arg)
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
	if (bprm->buf_args)
		kfree(bprm->buf_args);

	/* set new binary program */
	*bprm = bprm_new;
	bprm->dentry = dentry;

	/* prepare binary program */
	ret = prepare_binprm(bprm);
	if (ret < 0)
		return ret;

	/* load binary */
	return search_binary_handler(bprm);
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
