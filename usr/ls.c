#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <string.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>

#define STACK_GROW	64

#define SIGN(x)		(((x) > 0) - ((x) < 0))

static int nosort;
static int reverse = -1;
static int sort_time;
static int sort_size;

/*
 * Stack item = file.
 */
struct stack_item_t {
	char *			name;		/* file name */
	long			val;		/* file value : time, size... */
};

/*
 * Stack.
 */
struct stack_t {
	int 			size;		/* size */
	int 			capacity;	/* capacity */
	struct stack_item_t *	items;		/* items */
};

/*
 * Init a stack.
 */
static void stack_init(struct stack_t *stack)
{
	stack->size = 0;
	stack->capacity = 0;
	stack->items = NULL;
}

/*
 * Pop an item from a stack.
 */
static char *stack_pop(struct stack_t *stack)
{
	if (stack->size)
		return stack->items[--(stack->size)].name;

	return NULL;
}

/*
 * Grow a stack.
 */
static void stack_grow(struct stack_t *stack)
{
	stack->capacity += STACK_GROW;

	stack->items = (struct stack_item_t *) realloc(stack->items, sizeof(struct stack_item_t) * stack->capacity);
	if (stack->items)
		return;

	perror("stack_grow");
	exit(EXIT_FAILURE);
}

/*
 * Push an item on a stack
 */
static void stack_push(struct stack_t *stack, char *name, long val)
{
	/* grow stack */
	if (stack->size == stack->capacity)
		stack_grow(stack);

	/* push item */
	stack->items[stack->size].val = val;
	stack->items[stack->size++].name = name;
}

/*
 * Sort function.
 */
static int stack_sort_func(const void *p1, const void *p2)
{
	const struct stack_item_t *i1 = (const struct stack_item_t *) p1;
	const struct stack_item_t *i2 = (const struct stack_item_t *) p2;

	if (sort_time || sort_size)
		return SIGN(reverse * (i2->val - i1->val));

	return reverse * strcmp(i1->name, i2->name);
}

/*
 * Sort a stack.
 */
static void stack_sort(struct stack_t *stack)
{
	if (!nosort)
		qsort(stack->items, stack->size, sizeof(struct stack_item_t), stack_sort_func);
}

/*
 * Get files in a directory.
 */
static int getfiles(const char *dir_name, struct stack_t *stack)
{
	char fullname[PATH_MAX];
	size_t namelen, pathlen;
	struct dirent *entry;
	struct stat statbuf;
	int addslash;
	DIR *dirp;
	long val;

	/* get path length */
	pathlen = strlen(dir_name);
	addslash = dir_name[pathlen - 1] != '/';
	if (pathlen + addslash >= PATH_MAX)
		goto err_toolong;

	/* init fullname */
	memcpy(fullname, dir_name, pathlen + 1);
	if (addslash) {
		strcat(fullname + pathlen, "/");
		pathlen++;
	}

	/* open directory */
	dirp = opendir(dir_name);
	if (!dirp) {
		perror(dir_name);
		return -1;
	}

	/* read all entries */
	while ((entry = readdir(dirp)) != NULL) {
		/* check full name length */
		namelen = strlen(entry->d_name);
		if (pathlen + namelen >= PATH_MAX) {
			closedir(dirp);
			goto err_toolong;
		}

		/* set fullname */
		memcpy(fullname + pathlen, entry->d_name, namelen + 1);

		/* sort by time or size : stat file */
		val = 0;
		if (sort_time || sort_size)
			if (lstat(fullname, &statbuf) >= 0)
				val = sort_time ? statbuf.st_mtime : statbuf.st_size;

		/* push file on stack */
		stack_push(stack, strdup(fullname), val);
	}

	/* close directory */
	closedir(dirp);

	/* sort stack */
	stack_sort(stack);

	return 0;
err_toolong:
	fprintf(stderr, "Pathname too long\n");
	return -1;
}

int main()
{
	struct stack_t stack;
	char *name;

	stack_init(&stack);

	getfiles("./", &stack);

	while ((name = stack_pop(&stack)) != NULL)
		printf("%s\n", name);

	return 0;
}
