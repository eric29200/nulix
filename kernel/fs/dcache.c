#include <fs/fs.h>
#include <proc/sched.h>
#include <stdio.h>
#include <stderr.h>

#define D_HASHBITS     		10
#define D_HASHSIZE     		(1UL << D_HASHBITS)
#define D_HASHMASK     		(D_HASHSIZE - 1)

#define do_switch(x, y) 	do {					\
					__typeof__ (x) __tmp = x;	\
					x = y;				\
					y = __tmp;			\
				} while (0)

/* static variables */
static struct list_head dentry_hashtable[D_HASHSIZE];
static LIST_HEAD(dentry_unused);
static int dentry_nr_unused = 0;

/*
 * Init name hash.
 */
uint32_t init_name_hash()
{
	return 0;
}

/*
 * Compute name hash.
 */
uint32_t partial_name_hash(char c, uint32_t prev_hash)
{
	prev_hash = (prev_hash << 4) | (prev_hash >> (8 * sizeof(uint32_t) - 4));
	return prev_hash ^ c;
}

/*
 * End name hash.
 */
uint32_t end_name_hash(uint32_t hash)
{
	if (sizeof(hash) > sizeof(uint32_t))
		hash += hash >> 4 * sizeof(hash);

	return (uint32_t) hash;
}

/*
 * Compute hash.
 */
static struct list_head *d_hash(struct dentry * parent, uint32_t hash)
{
	hash += (uint32_t) parent;
	hash = hash ^ (hash >> D_HASHBITS) ^ (hash >> D_HASHBITS * 2);
	return dentry_hashtable + (hash & D_HASHMASK);
}

/*
 * Release dentry's inode.
 */
static int dentry_iput(struct dentry *dentry)
{
	struct inode *inode = dentry->d_inode;
	int ret;

	if (!inode)
		return 0;

	/* release inode */
	dentry->d_inode = NULL;
	list_del(&dentry->d_alias);
	INIT_LIST_HEAD(&dentry->d_alias);
	ret = inode->i_count == 1;
	iput(inode);

	return ret;
}

/*
 * Get a dentry.
 */
struct dentry *dget(struct dentry *dentry)
{
	if (dentry)
		dentry->d_count++;

	return dentry;
}

/*
 * Release a dentry.
 */
void dput(struct dentry *dentry)
{
	struct dentry *parent;

	if (!dentry)
		return;
repeat:
	/* update reference counter */
	dentry->d_count--;
	if (dentry->d_count < 0)
		panic("dput: negative d_count");

	/* still used */
	if (dentry->d_count)
		return;

	/* remove dentry from unused list */
	if (!list_empty(&dentry->d_lru)) {
		dentry_nr_unused--;
		list_del(&dentry->d_lru);
	}

	/* free dentry and release parent */
	if (list_empty(&dentry->d_hash)) {
		dentry_iput(dentry);
		parent = dentry->d_parent;
		d_free(dentry);
		if (dentry == parent)
			return;
		dentry = parent;
		goto repeat;
	}

	/* add dentry to unused list */
	list_add(&dentry->d_lru, &dentry_unused);
	dentry_nr_unused++;
}

/*
 * Allocate a dentry.
 */
struct dentry *d_alloc(struct dentry *parent, const struct qstr *name)
{
	struct dentry *dentry;
	char *str;

	/* allocate a new dentry */
	dentry = kmalloc(sizeof(struct dentry));
	if (!dentry)
		return NULL;

	/* allocate name (or use inline name) */
	if (name->len > DNAME_INLINE_LEN - 1) {
		str = kmalloc(name->len + 1);
		if (!str) {
			kfree(dentry);
			return NULL;
		}
	} else {
		str = dentry->d_iname;
	}

	/* set name */
	memcpy(str, name->name, name->len);
	str[name->len] = 0;

	/* set dentry */
	dentry->d_count = 1;
	dentry->d_inode = NULL;
	dentry->d_parent = NULL;
	dentry->d_sb = NULL;
	dentry->d_mounts = dentry;
	dentry->d_covers = dentry;
	dentry->d_name.name = str;
	dentry->d_name.len = name->len;
	dentry->d_name.hash = name->hash;
	INIT_LIST_HEAD(&dentry->d_hash);
	INIT_LIST_HEAD(&dentry->d_alias);
	INIT_LIST_HEAD(&dentry->d_lru);

	/* set parent */
	if (parent) {
		dentry->d_parent = dget(parent);
		dentry->d_sb = parent->d_sb;
	}

	return dentry;
}

/*
 * Allocate a root dentry.
 */
struct dentry *d_alloc_root(struct inode *root_inode)
{
	struct dentry *dentry;

	/* allocate a new dentry */
	dentry = d_alloc(NULL, &(const struct qstr) { "/", 1, 0 });
	if (!dentry)
		return NULL;

	/* set dentry */
	dentry->d_sb = root_inode->i_sb;
	dentry->d_parent = dentry;

	/* instantiate dentry */
	d_instantiate(dentry, root_inode);

	return dentry;
}

/*
 * Free a dentry.
 */
void d_free(struct dentry *dentry)
{
	if (dentry->d_name.name != dentry->d_iname)
		kfree(dentry->d_name.name);
	kfree(dentry);
}

/*
 * Instantiate a dentry.
 */
void d_instantiate(struct dentry *dentry, struct inode *inode)
{
	if (inode)
		list_add(&dentry->d_alias, &inode->i_dentry);

	dentry->d_inode = inode;
}

/*
 * Add a dentry.
 */
void d_add(struct dentry *dentry, struct inode *inode)
{
	struct dentry *parent = dentry->d_parent;
	list_add(&dentry->d_hash, d_hash(parent, dentry->d_name.hash));
	d_instantiate(dentry, inode);
}

/*
 * Drop a dentry = unhash.
 */
static void d_drop(struct dentry *dentry)
{
	list_del(&dentry->d_hash);
	INIT_LIST_HEAD(&dentry->d_hash);
}

/*
 * Delete a dentry.
 */
void d_delete(struct dentry *dentry)
{
	/* only one user : release inode */
	if (dentry->d_count == 1) {
		dentry_iput(dentry);
		return;
	}

	/* drop dentry */
	d_drop(dentry);
}

/*
 * Switch names.
 */
static void switch_names(struct dentry *dentry, struct dentry *target)
{
	char *old_name, *new_name;

	memcpy(dentry->d_iname, target->d_iname, DNAME_INLINE_LEN); 

	/* get old name */
	old_name = target->d_name.name;
	if (old_name == target->d_iname)
		old_name = dentry->d_iname;

	/* get new name */
	new_name = dentry->d_name.name;
	if (new_name == dentry->d_iname)
		new_name = target->d_iname;

	/* switch names */
	target->d_name.name = new_name;
	dentry->d_name.name = old_name;
}

/*
 * Move a dentry.
 */
void d_move(struct dentry *dentry, struct dentry *target)
{
	/* check inode */
	if (!dentry->d_inode)
		panic("d_move: moving negative dcache entry");

	/* move the dentry to the target hash queue */
	list_del(&dentry->d_hash);
	list_add(&dentry->d_hash, &target->d_hash);

	/* unhash the target: dput() will then get rid of it */
	list_del(&target->d_hash);
	INIT_LIST_HEAD(&target->d_hash);

	/* switch the parents and the names */
	switch_names(dentry, target);
	do_switch(dentry->d_parent, target->d_parent);
	do_switch(dentry->d_name.len, target->d_name.len);
	do_switch(dentry->d_name.hash, target->d_name.hash);
}

/*
 * Lookup in cache.
 */
struct dentry *d_lookup(struct dentry *parent, struct qstr *name)
{
	struct list_head *head = d_hash(parent, name->hash);
	struct list_head *tmp = head->next;
	struct dentry *dentry;

	while (tmp != head) {
		dentry = list_entry(tmp, struct dentry, d_hash);

		tmp = tmp->next;
		if (dentry->d_name.hash != name->hash)
			continue;
		if (dentry->d_parent != parent)
			continue;
		if (dentry->d_name.len != name->len)
			continue;
		if (memcmp(dentry->d_name.name, name->name, name->len))
			continue;

		return dget(dentry);
	}

	return NULL;
}

/*
 * Resolve a dentry full path.
 */
static char *d_path(struct dentry *dentry, char *buf, int len, int *error)
{
	struct dentry *root = current_task->fs->root, *parent;
	char *end = buf + len;
	int name_len;
	char *ret;

	/* reset error */
	*error = 0;

	/* end buffer */
	*--end = 0;
	len--;

	/* deleted dentry */
	if (dentry->d_parent != dentry && list_empty(&dentry->d_hash)) {
		len -= 10;
		end -= 10;
		memcpy(end, " (deleted)", 10);
	}

	/* get '/' right */
	ret = end - 1;
	*ret = '/';

	/* walk path */
	for (;;) {
		/* end */
		if (dentry == root)
			break;

		/* end */
		dentry = dentry->d_covers;
		parent = dentry->d_parent;
		if (dentry == parent)
			break;

		/* check buffer overflow */
		name_len = dentry->d_name.len;
		len -= name_len + 1;
		if (len < 0) {
			*error = -ENAMETOOLONG;
			break;
		}

		/* add component */
		end -= name_len;
		memcpy(end, dentry->d_name.name, name_len);
		*--end = '/';
		ret = end;

		/* go to parent */
		dentry = parent;
	}

	return ret;
}

/*
 * Get current working dir system call.
 */
int sys_getcwd(char *buf, size_t size)
{
	struct dentry *pwd = current_task->fs->pwd;
	int ret;

	/* current directory unlinked ? */
	if (pwd->d_parent != pwd && list_empty(&pwd->d_hash))
		return -ENOENT;

	/* resolve current working directory */
	buf = d_path(pwd, buf, size, &ret);

	return ret;
}

/*
 * Prune a dentry.
 */
static int prune_one_dentry(struct dentry *dentry)
{
	struct dentry * parent;
	int ret;

	/* prune dentry */
	list_del(&dentry->d_hash);
	ret = dentry_iput(dentry);
	parent = dentry->d_parent;
	d_free(dentry);

	/* release parent */
	if (parent != dentry)
		dput(parent);

	return ret;
}

/*
 * Prune dentries cache (returns number of inodes released).
 */
int prune_dcache(int dentries_count, int inodes_count)
{
	int inodes_freed = inodes_count;
	struct dentry *dentry;
	struct list_head *tmp;

	for (;;) {
		/* get next unused dentry */
		tmp = dentry_unused.prev;
		if (tmp == &dentry_unused)
			break;

		/* remove dentry from unused list */
		list_del(tmp);
		dentry = list_entry(tmp, struct dentry, d_lru);
		dentry_nr_unused--;
		INIT_LIST_HEAD(tmp);

		/* still used */
		if (dentry->d_count)
			continue;

		/* prune dentry */
		inodes_count -= prune_one_dentry(dentry);
		if (!inodes_count)
			break;
		if (!--dentries_count)
			break;
	}

	return inodes_freed - inodes_count;
}

/*
 * Shrink dentries for a super block.
 */
void shrink_dcache_sb(struct super_block * sb)
{
	struct list_head *pos, *n;
	struct dentry *dentry;

	list_for_each_safe(pos, n, &dentry_unused) {
		dentry = list_entry(pos, struct dentry, d_lru);

		if (dentry->d_sb != sb || dentry->d_count)
			continue;

		/* prune dentry */
		list_del(&dentry->d_lru);
		prune_one_dentry(dentry);
		dentry_nr_unused--;
	}
}

/*
 * Shrink dentries memory.
 */
void shrink_dcache_memory(int priority)
{
	int count = 0;

	/* compute number of dentries to shrink */
	if (priority > 1)
		count = dentry_nr_unused / priority;

	/* prune dentries */
	prune_dcache(count, -1);
}

/*
 * Init dcache.
 */
void init_dcache()
{
	size_t i;

	for (i = 0; i < D_HASHSIZE; i++)
		INIT_LIST_HEAD(&dentry_hashtable[i]);
}