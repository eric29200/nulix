#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <limits.h>
#include <time.h>
#include <stdbool.h>
#include <dirent.h>
#include <pwd.h>
#include <grp.h>
#include <sys/dev.h>
#include <sys/stat.h>

#define COLS			80
#define STACK_GROW_SIZE		64
#define LOGIN_NAME_MAX		256
#define GROUP_NAME_MAX		256
#define SIGN(val)		(((val) > 0) - ((val) < 0))

#define FLAG_LONG		(1 << 0)
#define FLAG_DIR		(1 << 1)
#define FLAG_INODE		(1 << 2)
#define FLAG_ALL		(1 << 3)
#define FLAG_ALLX		(1 << 4)
#define FLAG_CLASS		(1 << 5)
#define FLAG_MULTIPLE		(1 << 6)
#define FLAG_ONEPERLINE		(1 << 7)

#define FLAG_SORT_TIME		(1 << 0)
#define FLAG_SORT_SIZE		(1 << 1)
#define FLAG_SORT_NO		(1 << 2)
#define FLAG_SORT_REVERSE	(1 << 3)

/* global variables */
static int sort_flags = 0;
static char format[16] = "%s";
static int cols = 0;
static int col = 0;

/*
 * Stack entry.
 */
struct stack_entry_t {
	char *			name;
	nlink_t			nlink;
	uid_t			uid;
	gid_t 			gid;
	mode_t			mode;
	off_t			size;
	time_t			time;
	ino_t			ino;
	dev_t			rdev;
};

/*
 * Stack.
 */
struct stack_t {
	int			capacity;
	int			size;
	struct stack_entry_t *	entries;
};

/*
 * Init a stack.
 */
static void stack_init(struct stack_t *stack)
{
	stack->capacity = 0;
	stack->size = 0;
	stack->entries = NULL;
}

/*
 * Pop an entry from a stack.
 */
static struct stack_entry_t *stack_pop(struct stack_t *stack)
{
	if (!stack->size)
		return NULL;

	stack->size--;
	return &stack->entries[stack->size];
}

/*
  Grow a stack.
 */
static void stack_grow(struct stack_t *stack)
{
	struct stack_entry_t *buf;

	stack->capacity += STACK_GROW_SIZE;

	buf = (struct stack_entry_t *) realloc(stack->entries, sizeof(struct stack_entry_t) * stack->capacity);
	if (!buf) {
		perror("realloc");
		exit(1);
	}

	stack->entries = buf;
}

/*
 * Push an entry on a stack.
 */
static void stack_push(struct stack_t *stack, char *name, struct stat *st)
{
	/* grow stack if needed */
	if (stack->size == stack->capacity)
		stack_grow(stack);

	/* add entry */
	if (st) {
		stack->entries[stack->size].nlink = st->st_nlink;
		stack->entries[stack->size].uid = st->st_uid;
		stack->entries[stack->size].gid = st->st_gid;
		stack->entries[stack->size].mode = st->st_mode;
		stack->entries[stack->size].size = st->st_size;
		stack->entries[stack->size].time = st->st_mtime;
		stack->entries[stack->size].ino = st->st_ino;
		stack->entries[stack->size].rdev = st->st_rdev;
	}
	
	stack->entries[stack->size++].name = name;
}

/*
 * Push an entry on a stack.
 */
static void stack_push_entry(struct stack_t *stack, const struct stack_entry_t *entry)
{
	/* grow stack if needed */
	if (stack->size == stack->capacity)
		stack_grow(stack);

	/* add entry */
	stack->entries[stack->size].nlink = entry->nlink;
	stack->entries[stack->size].uid = entry->uid;
	stack->entries[stack->size].gid = entry->gid;
	stack->entries[stack->size].mode = entry->mode;
	stack->entries[stack->size].size = entry->size;
	stack->entries[stack->size].time = entry->time;
	stack->entries[stack->size].ino = entry->ino;
	stack->entries[stack->size].rdev = entry->rdev;
	stack->entries[stack->size++].name = entry->name;
}

/*
 * Compare 2 stack entries.
 */
static int stack_entry_compare(const void *a1, const void *a2)
{
	const struct stack_entry_t *e1 = (const struct stack_entry_t *) a1;
	const struct stack_entry_t *e2 = (const struct stack_entry_t *) a2;
	int reverse = (sort_flags & FLAG_SORT_REVERSE) ? -1 : 1;

	if (sort_flags & FLAG_SORT_TIME)
		return SIGN(reverse * (e1->time - e2->time));
	else if (sort_flags & FLAG_SORT_SIZE)
		return SIGN(reverse * (e1->size - e2->size));

	return reverse * strcmp(e2->name, e1->name);
}

/*
 * Sort a stack.
 */
static void stack_sort(struct stack_t *stack)
{
	if (!(sort_flags & FLAG_SORT_NO))
		qsort(stack->entries, stack->size, sizeof(struct stack_entry_t), stack_entry_compare);
}

/*
 * Dot directory ("." or "..") ?
 */
static bool dot_dir(const char *name)
{
	char *s = strrchr(name, '/');
	return s && s[1] == '.' && (!s[2] || (s[2] == '.' && !s[3]));
}

/*
 * Set file mode/permissions.
 */
static void mode_string(mode_t mode, char *buf)
{
	/* set file type */
	if (S_ISDIR(mode))
		buf[0] = 'd';
	else if (S_ISCHR(mode))
		buf[0] = 'c';
	else if (S_ISBLK(mode))
		buf[0] = 'b';
	else if (S_ISFIFO(mode))
		buf[0] = 'p';
	else if (S_ISLNK(mode))
		buf[0] = 'l';
	else if (S_ISSOCK(mode))
		buf[0] = 's';

	/* set permissions */
	if (mode & S_IRUSR)
		buf[1] = 'r';
	if (mode & S_IWUSR)
		buf[2] = 'w';
	if (mode & S_IXUSR)
		buf[3] = 'x';
	if (mode & S_IRGRP)
		buf[4] = 'r';
	if (mode & S_IWGRP)
		buf[5] = 'w';
	if (mode & S_IXGRP)
		buf[6] = 'x';
	if (mode & S_IROTH)
		buf[7] = 'r';
	if (mode & S_IWOTH)
		buf[8] = 'w';
	if (mode & S_IXOTH)
		buf[9] = 'x';

	/* set suid/sticky bit */
	if (mode & S_ISUID)
		buf[3] = (mode & S_IXUSR) ? 's' : 'S';
	if (mode & S_ISGID)
		buf[6] = (mode & S_IXGRP) ? 's' : 'S';
	if (mode & S_ISVTX)
		buf[9] = (mode & S_IXOTH) ? 't' : 'T';
}

/*
 * List a directory = populate stack_files.
 */
static int ls_dir(const struct stack_entry_t *entry, struct stack_t *stack_files, int flags)
{
	size_t namelen, pathlen = strlen(entry->name);
	char fullname[PATH_MAX];
	struct dirent *d;
	struct stat st;
	int addslash;
	DIR *dirp;

	/* check directory name length */
	addslash = entry->name[strlen(entry->name) - 1] != '/';
	if (pathlen + addslash >= PATH_MAX)
		goto err_path_too_long;

	/* prepare filename with directory name */
	memcpy(fullname, entry->name, pathlen + 1);
	if (addslash) {
		fullname[pathlen++] = '/';
		fullname[pathlen] = 0;
	}

	/* open directory */
	dirp = opendir(entry->name);
	if (!dirp) {
		perror(entry->name);
		return 1;
	}

	/* list entries */
	while ((d = readdir(dirp))) {
		/* skip hidden files */
		if (!(flags & (FLAG_ALL | FLAG_ALLX)) && *d->d_name == '.')
			continue;

		/* skip "." and ".." */
		if (!(flags & FLAG_ALL) && (strcmp(d->d_name, ".") == 0 || strcmp(d->d_name, "..") == 0))
			continue;

		/* check path length */
		namelen = strlen(d->d_name);
		if (pathlen + namelen >= PATH_MAX) {
			closedir(dirp);
			goto err_path_too_long;
		}

		/* build filename */
		memcpy(fullname + pathlen, d->d_name, namelen + 1);

		/* stat file */
		if (lstat(fullname, &st) < 0) {
			perror(fullname);
			goto err;
		}

		/* push file */
		stack_push(stack_files, strdup(fullname), &st);
	}

	/* close directory */
	closedir(dirp);

	/* sort files */
	stack_sort(stack_files);

	return 0;
err_path_too_long:
	fprintf(stderr, "%s: path too long\n", entry->name);
err:
	return 1;
}

/*
 * Output an entry.
 */
static void ls_entry(const struct stack_entry_t *entry, int flags)
{
	char buf[PATH_MAX], *filename, *classp;
	char mode[] = "----------";
	struct passwd *pwd;
	struct group *grp;
	struct tm *tm;
	ssize_t len;
	char class;

	/* print inode */
	if (flags & FLAG_INODE)
		printf("%5lu ", entry->ino);

	/* ls -l */
	if (flags & FLAG_LONG) {
		/* print mode */
		mode_string(entry->mode, mode);
		printf("%s ", mode);

		/* print number of links */
		printf("%4ld ", entry->nlink);

		/* print user */
		pwd = getpwuid(entry->uid);
		if (pwd)
			snprintf(buf, sizeof(buf), "%s ", pwd->pw_name);
		else
			snprintf(buf, sizeof(buf), "%d ", entry->uid);
		printf("%-8.8s ", buf);

		/* print group */
		grp = getgrgid(entry->gid);
		if (grp)
			snprintf(buf, sizeof(buf), "%s ", grp->gr_name);
		else
			snprintf(buf, sizeof(buf), "%d ", entry->gid);
		printf("%-8.8s ", buf);

		/* print size or major/minor numbers */
		if (S_ISBLK(entry->mode) || S_ISCHR(entry->mode))
			printf("%4u, %4u ", major(entry->rdev), minor(entry->rdev));
		else
			printf("%10lu ", entry->size);

		/* get time */
		tm = localtime(&entry->time);
		if (tm) {
			if (time(NULL) > entry->time + (180 * 24 * 60 * 60))
				strftime(buf, sizeof(buf), "%b %d  %Y", tm);
			else
				strftime(buf, sizeof(buf), "%b %d %H:%M", tm);
		} else {
			snprintf(buf, sizeof(buf), "%lld", entry->time);
		}

		/* print time */
		printf("%s ", buf);
	}

	/* get file class */
	class = 0;
	if (flags & FLAG_CLASS) {
		if (S_ISLNK(entry->mode))
			class = '@';
		else if (S_ISDIR(entry->mode))
			class = '/';
		else if (S_IEXEC & entry->mode)
			class = '*';
		else if (S_ISFIFO(entry->mode))
			class = '|';
		else if (S_ISSOCK(entry->mode))
			class = '=';
	}


	/* copy filename to buffer */
	len = strlen(entry->name);
	memcpy(buf, entry->name, len + 1);
	filename = strrchr(buf, '/');
	if (filename)
		filename++;
	else
		filename = entry->name;

	/* add class */
	if (class) {
		classp = &buf[len];
		*classp++ = class;
		*classp++ = 0;
	}

	/* print filename */
	printf(format, filename);

	/* print link target */
	if (S_ISLNK(entry->mode)) {
		len = readlink(entry->name, buf, sizeof(buf) - 1);
		if (len > 0) {
			buf[len] = 0;
			printf(" -> %s", buf);
		}
	}

	/* print end of line */
	if ((flags & (FLAG_LONG | FLAG_ONEPERLINE)) || ++col == cols) {
		col = 0;
		fputc('\n', stdout);
	}
}

/*
 * Set filename format.
 */
static void set_name_format(struct stack_t *stack, int flags)
{
	int maxlen = 0, maxlen2, len, i;
	char *s;

	/* ls -l = default format "%s" */
	if (flags & (FLAG_LONG | FLAG_ONEPERLINE))
		return;

	/* find maximum filename length */
	for (i = 0; i < stack->size; i++) {
		s = strrchr(stack->entries[i].name, '/');
		if (s)
			s++;
		else
			s = stack->entries[i].name;

		len = strlen(s);
		if (len > maxlen)
			maxlen = len;
	}

	/* limit number of columns */
	maxlen += 2;
	maxlen2 = flags & FLAG_INODE ? maxlen + 6 : maxlen;
	cols = (COLS - 1) / maxlen2;

	/* set format */
	sprintf(format, "%%-%d.%ds", maxlen, maxlen);
}

/*
 * Usage.
 */
static void usage(const char *name)
{
	fprintf(stderr, "Usage: %s [-ldiaAF1RtsrU] [name ...]\n", name);
}

int main(int argc, char **argv)
{
	struct stack_t stack_files, stack_dirs;
	int flags = 0, recursive = 1, ret = 0;
	struct stack_entry_t *entry;
	char *def[] = { ".", NULL };
	const char *name = argv[0];
	struct stat st;
	char *p;

	/* skip program name */
	argc--;
	argv++;

	/* init stacks */
	stack_init(&stack_files);
	stack_init(&stack_dirs);
	
	/* parse options */
	while (*argv && argv[0][0] == '-') {
		for (p = &argv[0][1]; *p; p++) {
			switch (*p) {
				case 'l':
					flags |= FLAG_LONG;
					break;
				case 'd':
					flags |= FLAG_DIR;
					recursive = 0;
					break;
				case 'i':
					flags |= FLAG_INODE;
					break;
				case 'a':
					flags |= FLAG_ALL;
					break;
				case 'A':
					flags |= FLAG_ALLX;
					break;
				case 'F':
					flags |= FLAG_CLASS;
					break;
				case '1':
					flags |= FLAG_ONEPERLINE;
					break;
				case 'R':
					recursive = -1;
					break;
				case 't':
					sort_flags |= FLAG_SORT_TIME;
					break;
				case 'S':
					sort_flags |= FLAG_SORT_SIZE;
					break;
				case 'r':
					sort_flags |= FLAG_SORT_REVERSE;
					break;
				case 'U':
					sort_flags |= FLAG_SORT_NO;
					break;
				default:
					usage(name);
					exit(1);
			}
		}

		argc--;
		argv++;
	}

	/* no argument : ls . */
	if (!argc) {
		argv = def;
		argc = 1;
	}

	/* multiple arguments ? */
	if (argc > 1)
		flags |= FLAG_MULTIPLE;

	/* not a tty : one per line */
	if (!isatty(STDOUT_FILENO))
		flags |= FLAG_ONEPERLINE;

	/* make entries */
	for (; *argv; argv++) {
		/* stat argument */
		if (lstat(*argv, &st) < 0) {
			perror(*argv);
			ret = 1;
			continue;
		}

		/* add it to directories list or files list */
		if (recursive && S_ISDIR(st.st_mode))
			stack_push(&stack_dirs, strdup(*argv), &st);
		else
			stack_push(&stack_files, strdup(*argv), &st);
	}

	/* default : recursive first level */
	if (recursive)
		recursive--;

	/* sort stack */
	stack_sort(&stack_files);

	/* ls all files/dirs */
	while (stack_files.size || stack_dirs.size) {
		/* set filename format */
		set_name_format(&stack_files, flags);

		/* ls files */
		while (stack_files.size) {
			/* get next file */
			entry = stack_pop(&stack_files);

			/* list file */
			ls_entry(entry, flags);

			/* directory : push it on dirs stack */
			if (S_ISDIR(entry->mode) && recursive && !dot_dir(entry->name))
				stack_push_entry(&stack_dirs, entry);
			else
				free(entry->name);
		}

		/* ls dirs */
		if (stack_dirs.size) {
			/* get next directory */
			entry = stack_pop(&stack_dirs);

			/* list directory = populate files list */
			if (ls_dir(entry, &stack_files, flags)) {
				ret = 1;
			} else if (strcmp(entry->name, ".") != 0) {
				/* new line */
				if (col) {
					col = 0;
					fputc('\n', stdout);
				}

				/* print directory name */
				if ((flags & FLAG_MULTIPLE) || recursive)
					printf("\n%s:\n", entry->name);
			}

			/* free directory name */
			free(entry->name);

			/* stop recursive list ? */
			if (recursive)
				recursive--;
		}
	}

	/* last line */
	if (!(flags & (FLAG_LONG || FLAG_ONEPERLINE)) && col)
		fputc('\n', stdout);

	return ret;
}
