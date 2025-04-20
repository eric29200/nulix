#include <fs/devpts_fs.h>
#include <string.h>
#include <fcntl.h>

/* root entry */
static struct devpts_entry *root_entry = NULL;
static ino_t devpts_next_ino = DEVPTS_ROOT_INO;

/*
 * Allocate a new entry.
 */
static struct devpts_entry *devpts_alloc_entry(const char *name, size_t name_len, mode_t mode)
{
	struct devpts_entry *de;

	/* allocate new entry */
	de = (struct devpts_entry *) kmalloc(sizeof(struct devpts_entry));
	if (!de)
		return NULL;

	/* set entry */
	memset(de, 0, sizeof(struct devpts_entry));
	de->ino = devpts_next_ino++;
	de->name_len = name_len;
	de->mode = mode;

	/* allocate name */
	de->name = (char *) kmalloc(name_len + 1);
	if (!de->name)
		goto err;

	/* set name */
	memcpy(de->name, name, name_len);
	de->name[name_len] = 0;

	return de;
err:
	kfree(de);
	return NULL;
}

/*
 * Get root entry.
 */
struct devpts_entry *devpts_get_root_entry()
{
	if (root_entry)
		return root_entry;

	/* no root yet : create it */
	root_entry = devpts_alloc_entry("/", 1, S_IFDIR | 0755);
	if (!root_entry)
		return NULL;

	/* init entries */
	INIT_LIST_HEAD(&root_entry->u.dir.entries);

	return root_entry;
}

/*
 * Test if a name matches a directory entry.
 */
static inline int devpts_name_match(const char *name, size_t len, struct devpts_entry *de)
{
	return len == de->name_len && strncmp(name, de->name, len) == 0;
}

/*
 * Find an entry inside a directory.
 */
static struct devpts_entry *devpts_find_entry(struct devpts_entry *dir, const char *name, size_t name_len)
{
	struct devpts_entry *de;
	struct list_head *pos;

	/* "." and ".." entries */
	if (name_len == 1 && name[0] == '.')
		return dir;
	else if (name_len == 2 && name[0] == '.' && name[1] == '.')
		return dir->parent;

	/* find entry */
	list_for_each(pos, &dir->u.dir.entries) {
		de = list_entry(pos, struct devpts_entry, list);
		if (devpts_name_match(name, name_len, de))
			return de;
	}

	return NULL;
}

/*
 * Find an entry.
 */
struct devpts_entry *devpts_find(struct devpts_entry *dir, const char *name, size_t name_len)
{
	/* use root directory */
	if (!dir)
		dir = devpts_get_root_entry();

	/* dir must be a directory */
	if (!dir || !S_ISDIR(dir->mode))
		return NULL;

	/* find entry */
	return devpts_find_entry(dir, name, name_len);
}

/*
 * Register a device.
 */
struct devpts_entry *devpts_register(const char *name, mode_t mode, dev_t dev)
{
	struct devpts_entry *root, *de;
	size_t name_len;

	/* must be a device */
	if (!S_ISCHR(mode) && !S_ISBLK(mode))
		return NULL;

	/* get root directory */
	root = devpts_get_root_entry();

	/* check if entry exists */
	name_len = strlen(name);
	if (devpts_find_entry(root, name, name_len))
		return NULL;

	/* allocate new entry */
	de = devpts_alloc_entry(name, name_len, mode);
	if (!de)
		return NULL;

	/* set device */
	de->u.dev.dev = dev;

	/* add entry */
	de->parent = root;
	list_add_tail(&de->list, &root->u.dir.entries);

	return de;
}

/*
 * Unregister an entry.
 */
void devpts_unregister(struct devpts_entry *de)
{
	/* can't unregister directories */
	if (!de || S_ISDIR(de->mode))
		return;

	/* remove entry */
	list_del(&de->list);

	/* free entry */
	if (de->name)
		kfree(de->name);
	kfree(de);
}
