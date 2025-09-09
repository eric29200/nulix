#include <fs/fs.h>
#include <stdio.h>

#define D_HASHBITS     10
#define D_HASHSIZE     (1UL << D_HASHBITS)
#define D_HASHMASK     (D_HASHSIZE-1)

#define switch(x, y) 		do {					\
					__typeof__ (x) __tmp = x;	\
					x = y;				\
					y = __tmp;			\
				} while (0)

/* static variables */
static struct list_head dentry_hashtable[D_HASHSIZE];

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
	if (dentry)
		dentry->d_count--;
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

	/* allocate name */
	str = kmalloc(name->len + 1);
	if (!str) {
		kfree(dentry);
		return NULL;
	}

	/* set name */
	memcpy(str, name->name, name->len);
	str[name->len] = 0;

	/* set dentry */
	dentry->d_count = 0;
	dentry->d_inode = NULL;
	dentry->d_parent = parent;
	dentry->d_mounts = dentry;
	dentry->d_covers = dentry;
	dentry->d_name.name = str;
	dentry->d_name.len = name->len;
	dentry->d_name.hash = name->hash;
	INIT_LIST_HEAD(&dentry->d_hash);

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
	dentry->d_count = 1;
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
	kfree(dentry->d_name.name);
	kfree(dentry);
}

/*
 * Instantiate a dentry.
 */
void d_instantiate(struct dentry *dentry, struct inode *inode)
{
	dentry->d_inode = inode;
}

/*
 * Add a dentry.
 */
void d_add(struct dentry *dentry, struct inode *inode)
{
	struct dentry *parent = dget(dentry->d_parent);

	list_add(&dentry->d_hash, d_hash(parent, dentry->d_name.hash));
	d_instantiate(dentry, inode);
}

/*
 * Delete a dentry.
 */
void d_delete(struct dentry *dentry)
{
	struct inode *inode = dentry->d_inode;

	dentry->d_inode = NULL;
	iput(inode);
}

/*
 * Move a dentry.
 */
void d_move(struct dentry *dentry, struct dentry *target)
{
	struct list_head *old_head;

	/* check inode */
	if (!dentry->d_inode)
		panic("d_move: moving negative dcache entry");

	/* switch the hashes.. */
	old_head = dentry->d_hash.prev;
	list_del(&dentry->d_hash);
	list_add(&dentry->d_hash, &target->d_hash);
	list_del(&target->d_hash);
	list_add(&target->d_hash, old_head);

	/* switch names */
	switch(dentry->d_parent, target->d_parent);
	switch(dentry->d_name.name, target->d_name.name);
	switch(dentry->d_name.len, target->d_name.len);
	switch(dentry->d_name.hash, target->d_name.hash);

	/* delete target */
	d_delete(target);
}

/*
 * Lookup in cache.
 */
static struct dentry *__dlookup(struct list_head *head, struct dentry *parent, struct qstr *name)
{
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

		return dentry;
	}
	return NULL;
}

/*
 * Lookup in cache.
 */
struct dentry * d_lookup(struct dentry *dir, struct qstr *name)
{
	return __dlookup(d_hash(dir, name->hash), dir, name);
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