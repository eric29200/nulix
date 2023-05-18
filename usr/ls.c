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
#include <termios.h>
#include <getopt.h>
#include <sys/ioctl.h>
#include <sys/dev.h>
#include <sys/stat.h>

#include "libutils/utils.h"

#define OPT_COLOR		((OPT_HELP) + 100)

#define STACK_GROW_SIZE		64
#define LOGIN_NAME_MAX		256
#define GROUP_NAME_MAX		256
#define SIGN(val)		(((val) > 0) - ((val) < 0))

#define COLOR_NO		""
#define COLOR_LINK		"\033[1;36m"
#define COLOR_DIR		"\033[1;34m"
#define COLOR_DEV		"\033[1;33m"
#define COLOR_FIFO		"\033[33m"
#define COLOR_SOCK		"\033[1;35m"
#define COLOR_EXE		"\033[1;32m"
#define COLOR_DEFAULT		"\033[0m"

#define FLAG_LONG		(1 << 0)
#define FLAG_DIR		(1 << 1)
#define FLAG_INODE		(1 << 2)
#define FLAG_ALL		(1 << 3)
#define FLAG_ALLX		(1 << 4)
#define FLAG_CLASS		(1 << 5)
#define FLAG_MULTIPLE		(1 << 6)
#define FLAG_ONEPERLINE		(1 << 7)
#define FLAG_HUMAN_READABLE	(1 << 8)
#define FLAG_COLOR		(1 << 9)

#define FLAG_SORT_TIME		(1 << 0)
#define FLAG_SORT_SIZE		(1 << 1)
#define FLAG_SORT_NO		(1 << 2)
#define FLAG_SORT_REVERSE	(1 << 3)

/* global variables */
static int sort_flags = 0;
static int nr_cols = 80;
static int nr_entries_per_line = 0;
static int entries_line_cpt = 0;

/* formats */
static char format_name[16];
static char format_inode[16];
static char format_size[16];
static char format_size_str[16];
static char format_nlink[16];
static char format_major[16];
static char format_minor[16];

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

	return reverse * strcasecmp(e2->name, e1->name);
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
	char buf[PATH_MAX], *filename, *classp, *color_start, *color_end;
	char mode[] = "----------";
	struct passwd *pwd;
	struct group *grp;
	struct tm *tm;
	ssize_t len;
	char class;

	/* print inode */
	if (flags & FLAG_INODE)
		printf(format_inode, entry->ino);

	/* ls -l */
	if (flags & FLAG_LONG) {
		/* print mode */
		mode_string(entry->mode, mode);
		printf("%s ", mode);

		/* print number of links */
		printf(format_nlink, entry->nlink);

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
		else if (flags & FLAG_HUMAN_READABLE)
			printf(format_size_str, __human_size(entry->size, buf, sizeof(buf)));
		else
			printf(format_size, entry->size);

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

	/* choose color */
	color_start = color_end = COLOR_NO;
	if (flags & FLAG_COLOR) {
		if (S_ISLNK(entry->mode))
			color_start = COLOR_LINK;
		else if (S_ISDIR(entry->mode))
			color_start = COLOR_DIR;
		else if (S_ISFIFO(entry->mode))
			color_start = COLOR_FIFO;
		else if (S_ISSOCK(entry->mode))
			color_start = COLOR_SOCK;
		else if (S_ISCHR(entry->mode) || S_ISBLK(entry->mode))
			color_start = COLOR_DEV;
		else if (S_IEXEC & entry->mode)
			color_start = COLOR_EXE;

		if (*color_start)
			color_end = COLOR_DEFAULT;
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
	printf("%s", color_start);
	printf(format_name, filename);
	printf("%s", color_end);

	/* print link target */
	if (S_ISLNK(entry->mode) & (flags & FLAG_LONG)) {
		len = readlink(entry->name, buf, sizeof(buf) - 1);
		if (len > 0) {
			buf[len] = 0;
			printf(" -> %s", buf);
		}
	}

	/* print end of line */
	if ((flags & (FLAG_LONG | FLAG_ONEPERLINE)) || ++entries_line_cpt == nr_entries_per_line) {
		entries_line_cpt = 0;
		fputc('\n', stdout);
	}
}

/*
 * Set formats.
 */
static void set_formats(struct stack_t *stack, int flags)
{
	static char buf[32];
	size_t len, len1, len2, inode_len = 0, max_name = 0;
	dev_t max_major = 0, max_minor = 0;
	nlink_t max_nlink = 0;
	off_t max_size = 0;
	ino_t max_ino = 0;
	char *s;
	int i;

	/* find maximum values */
	for (i = 0; i < stack->size; i++) {
		/* find maximum attributes */
		if (stack->entries[i].ino > max_ino)
			max_ino = stack->entries[i].ino;
		if (stack->entries[i].size > max_size)
			max_size = stack->entries[i].size;
		if (stack->entries[i].nlink > max_nlink)
			max_nlink = stack->entries[i].nlink;
		if (S_ISBLK(stack->entries[i].mode) || S_ISCHR(stack->entries[i].mode)) {
			if (major(stack->entries[i].rdev) > max_major)
				max_major = major(stack->entries[i].rdev);
			if (minor(stack->entries[i].rdev) > max_minor)
				max_minor = minor(stack->entries[i].rdev);
		}

		/* find maximum name length */
		s = strrchr(stack->entries[i].name, '/');
		if (s)
			s++;
		else
			s = stack->entries[i].name;

		len = strlen(s);
		if (len > max_name)
			max_name = len;
	}

	/* set inode format */
	if (flags & FLAG_INODE) {
		inode_len = len = snprintf(buf, sizeof(buf), "%lu", max_ino);
		sprintf(format_inode, "%%%dlu ", len);
	}

	if (flags & FLAG_LONG) {
		/* set major format */
		len1 = snprintf(buf, sizeof(buf), "%lu,", max_major);
		sprintf(format_major, "%%%dd ", len1);

		/* set minor format */
		len2 = snprintf(buf, sizeof(buf), "%lu ", max_minor);
		sprintf(format_minor, "%%%dd ", len2);

		/* set size format */
		len = snprintf(buf, sizeof(buf), "%lu", max_size);
		if (len > len1 + len2) {
			sprintf(format_size, "%%%dd ", len);
			sprintf(format_size_str, "%%%d.%ds ", len, len);
		} else {
			sprintf(format_size, "%%%dd ", len1 + len2);
			sprintf(format_size_str, "%%%d.%ds ", len1 + len2, len1 + len2);
		}

		/* set nlink format */
		len = snprintf(buf, sizeof(buf), "%lu", max_nlink);
		sprintf(format_nlink, "%%%dd ", len, len);
	}

	/* set name format */
	if (flags & (FLAG_LONG | FLAG_ONEPERLINE)) {
		sprintf(format_name, "%%s");
	} else {
		/* limit number of columns */
		max_name += 2;
		nr_entries_per_line = (nr_cols - 1) / (flags & FLAG_INODE ? max_name + inode_len + 1 : max_name);

		/* set format */
		sprintf(format_name, "%%-%d.%ds", max_name, max_name);
	}
}

/*
 * Usage.
 */
static void usage(const char *name)
{
	fprintf(stderr, "Usage: %s [-ldiaAF1RtSrU] [name ...]\n", name);
	fprintf(stderr, "\t  , --help\t\t\tprint help and exit\n");
	fprintf(stderr, "\t-l,\t\t\t\tuse a long listing format\n");
	fprintf(stderr, "\t-d, --directory\t\t\tlist directories themselves, not their contents\n");
	fprintf(stderr, "\t-i, --inode\t\t\tprint the inode number of each file\n");
	fprintf(stderr, "\t-a, --all\t\t\tdo not ignore entries starting with .\n");
	fprintf(stderr, "\t-A, --almost-all\t\tdo not list implied . and ..\n");
	fprintf(stderr, "\t-F, --classify\t\t\tappend class indicator\n");
	fprintf(stderr, "\t-1,\t\t\t\tlist one file per line\n");
	fprintf(stderr, "\t-R, --recursive\t\t\tlist subdirectories recursiverly\n");
	fprintf(stderr, "\t-t,\t\t\t\tsort by time\n");
	fprintf(stderr, "\t-S,\t\t\t\tsort by size\n");
	fprintf(stderr, "\t-r, --reverse\t\t\treverse order while sorting\n");
	fprintf(stderr, "\t-U,\t\t\t\tdo not sort\n");
	fprintf(stderr, "\t-h, --human-readable\t\tprint sizes in human readable format\n");
}

/* options */
struct option long_opts[] = {
	{ "help",		no_argument,	0,	OPT_HELP	},
	{ "color",		no_argument,	0,	OPT_COLOR	},
	{ 0,			no_argument,	0,	'l'		},
	{ "directory",		no_argument,	0,	'd'		},
	{ "inode",		no_argument,	0,	'i'		},
	{ "all",		no_argument,	0,	'a'		},
	{ "almost-all",		no_argument,	0,	'A'		},
	{ "classify",		no_argument,	0,	'F'		},
	{ 0,			no_argument,	0,	'1'		},
	{ "recursive",		no_argument,	0,	'R'		},
	{ 0,			no_argument,	0,	't'		},
	{ 0,			no_argument,	0,	'S'		},
	{ "reverse",		no_argument,	0,	'r'		},
	{ 0,			no_argument,	0,	'U'		},
	{ "human-readable",	no_argument,	0,	'h'		},
	{ 0,			0,		0,	0		},
};

int main(int argc, char **argv)
{
	int flags = 0, recursive = 1, ret = 0, c;
	struct stack_t stack_files, stack_dirs;
	struct stack_entry_t *entry;
	char *def[] = { ".", NULL };
	const char *name = argv[0];
	struct winsize ws;
	struct stat st;

	/* get options */
	while ((c = getopt_long(argc, argv, "ldiaAF1RtSrUh", long_opts, NULL)) != -1) {
		switch (c) {
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
			case 'h':
				flags |= FLAG_HUMAN_READABLE;
				break;
			case OPT_COLOR:
				flags |= FLAG_COLOR;
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
			case OPT_HELP:
				usage(name);
				exit(0);
				break;
			default:
				exit(1);
				break;
		}
	}

	/* skip options */
	argc -= optind;
	argv += optind;

	/* init stacks */
	stack_init(&stack_files);
	stack_init(&stack_dirs);
	
	/* no argument : ls . */
	if (!argc) {
		argv = def;
		argc = 1;
	}

	/* multiple arguments ? */
	if (argc > 1)
		flags |= FLAG_MULTIPLE;

	/* not a tty : one entry per line */
	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) < 0)
		flags |= FLAG_ONEPERLINE;
	else
		nr_cols = ws.ws_col;

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
		/* set formats */
		set_formats(&stack_files, flags);

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
				if (entries_line_cpt) {
					entries_line_cpt = 0;
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
	if (!(flags & (FLAG_LONG || FLAG_ONEPERLINE)) && entries_line_cpt)
		fputc('\n', stdout);

	return ret;
}
