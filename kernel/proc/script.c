#include <proc/sched.h>
#include <fs/fs.h>
#include <stderr.h>
#include <fcntl.h>

/*
 * Load a script.
 */
int script_load(const char *path, struct binargs_t *bargs)
{
	char *s, *interp, *interp_name, *i_arg = NULL;
	struct binargs_t bargs_new;
	struct file_t *filp;
	char buf[128];
	int fd, ret;
	ssize_t n;

	/* open file */
	fd = do_open(AT_FDCWD, path, O_RDONLY, 0);
	if (fd < 0)
		return fd;
	filp = current_task->files->filp[fd];

	/* read first characters */
	n = do_read(filp, buf, 127);
	buf[128] = 0;

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

	/* create new binary arguments */
	bargs_new.argc = 2 + (i_arg ? 1 : 0);
	bargs_new.argv_len = strlen(interp_name) + 1 + (i_arg ? strlen(i_arg) + 1: 0) + strlen(path) + 1;
	bargs_new.envc = bargs->envc;
	bargs_new.envp_len = bargs->envp_len;
	bargs_new.dont_free = 0;

	/* allocate buffer */
	bargs_new.buf = bargs_new.p = (char *) kmalloc(bargs_new.argv_len + bargs_new.envp_len);
	if (!bargs_new.buf)
		return -ENOMEM;

	/* copy arguments */
	copy_strings(&bargs_new, 1, &interp_name);
	copy_strings(&bargs_new, 1, (char **) &path);
	if(i_arg)
		copy_strings(&bargs_new, 1, &i_arg);

	/* copy environ */
	memcpy(bargs_new.p, bargs->buf + bargs->argv_len, bargs->envp_len);

	/* free old binary arguments */
	if (bargs->buf) {
		bargs->dont_free = 1;
		kfree(bargs->buf);
	}

	/* load binary */
	ret = binary_load(interp, &bargs_new);

	if (!bargs_new.dont_free)
		kfree(bargs_new.buf);

	return ret;
}