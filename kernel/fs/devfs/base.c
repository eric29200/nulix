#include <fs/dev_fs.h>
#include <fcntl.h>

/* root entry */
static struct devfs_entry_t *root_entry = NULL;
static ino_t devfs_next_ino = DEVFS_ROOT_INO;

/*
 * Test if a name matches a directory entry.
 */
static inline int devfs_name_match(const char *name, size_t len, struct devfs_entry_t *de)
{
	return len == de->name_len && strncmp(name, de->name, len) == 0;
}

/*
 * Find an entry inside a directory.
 */
static struct devfs_entry_t *devfs_find_entry(struct devfs_entry_t *dir, const char *name, size_t name_len)
{
	struct devfs_entry_t *de;
	struct list_head_t *pos;

	/* "." and ".." entries */
	if (name_len == 1 && name[0] == '.')
		return dir;
	else if (name_len == 2 && name[0] == '.' && name[1] == '.')
		return dir->parent;

	/* find entry */
	list_for_each(pos, &dir->u.dir.entries) {
		de = list_entry(pos, struct devfs_entry_t, list);
		if (devfs_name_match(name, name_len, de))
			return de;
	}

	return NULL;
}

/*
 * Allocate a new entry.
 */
static struct devfs_entry_t *devfs_alloc_entry(const char *name, size_t name_len, mode_t mode)
{
	struct devfs_entry_t *de;

	/* allocate new entry */
	de = (struct devfs_entry_t *) kmalloc(sizeof(struct devfs_entry_t));
	if (!de)
		return NULL;

	/* set entry */
	memset(de, 0, sizeof(struct devfs_entry_t));
	de->ino = devfs_next_ino++;
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
 * Free an entry.
 */
static void devfs_free_entry(struct devfs_entry_t *de)
{
	if (!de)
		return;

	if (de->name)
		kfree(de->name);

	kfree(de);
}

/*
 * Get root entry.
 */
struct devfs_entry_t *devfs_get_root_entry()
{
	if (root_entry)
		return root_entry;

	/* no root yet : create it */
	root_entry = devfs_alloc_entry("/", 1, S_IFDIR | 0755);
	if (!root_entry)
		return NULL;

	/* init entries */
	INIT_LIST_HEAD(&root_entry->u.dir.entries);

	return root_entry;
}

/*
 * Find an entry.
 */
struct devfs_entry_t *devfs_find(struct devfs_entry_t *dir, const char *name, size_t name_len)
{
	/* use root directory */
	if (!dir)
		dir = devfs_get_root_entry();

	/* dir must be a directory */
	if (!dir || !S_ISDIR(dir->mode))
		return NULL;

	/* find entry */
	return devfs_find_entry(dir, name, name_len);
}

/*
 * Register a device.
 */
struct devfs_entry_t *devfs_register(struct devfs_entry_t *dir, const char *name, mode_t mode, dev_t dev)
{
	struct devfs_entry_t *de;
	size_t name_len;

	/* must be a device */
	if (!S_ISCHR(mode) && !S_ISBLK(mode))
		return NULL;

	/* use root directory */
	if (!dir)
		dir = devfs_get_root_entry();

	/* dir must be a directory */
	if (!dir || !S_ISDIR(dir->mode))
		return NULL;

	/* check if entry exists */
	name_len = strlen(name);
	if (devfs_find_entry(dir, name, name_len))
		return NULL;

	/* allocate new entry */
	de = devfs_alloc_entry(name, name_len, mode);
	if (!de)
		return NULL;

	/* set device */
	de->u.dev.dev = dev;

	/* add entry to parent */
	de->parent = dir;
	list_add_tail(&de->list, &dir->u.dir.entries);

	return de;
}

/*
 * Unregister an entry.
 */
void devfs_unregister(struct devfs_entry_t *de)
{
	/* can't unregister directories */
	if (!de || S_ISDIR(de->mode))
		return;

	/* free link name */
	if (S_ISLNK(de->mode) && de->u.symlink.linkname)
		kfree(de->u.symlink.linkname);

	/* remove entry */
	list_del(&de->list);

	/* free entry */
	devfs_free_entry(de);
}

/*
 * Create a directory.
 */
struct devfs_entry_t *devfs_mkdir(struct devfs_entry_t *dir, const char *name)
{
	struct devfs_entry_t *de;
	size_t name_len;

	/* use root directory */
	if (!dir)
		dir = devfs_get_root_entry();

	/* dir must be a directory */
	if (!dir || !S_ISDIR(dir->mode))
		return NULL;

	/* check if entry exists */
	name_len = strlen(name);
	if (devfs_find_entry(dir, name, name_len))
		return NULL;

	/* allocate new entry */
	de = devfs_alloc_entry(name, name_len, S_IFDIR | 0755);
	if (!de)
		return NULL;

	/* set entries */
	INIT_LIST_HEAD(&de->u.dir.entries);

	/* add entry to parent */
	de->parent = dir;
	list_add_tail(&de->list, &dir->u.dir.entries);

	return de;
}

/*
 * Create a symbolic link.
 */
struct devfs_entry_t *devfs_symlink(struct devfs_entry_t *dir, const char *name, const char *linkname)
{
	struct devfs_entry_t *de;
	size_t name_len;

	/* use root directory */
	if (!dir)
		dir = devfs_get_root_entry();

	/* dir must be a directory */
	if (!dir || !S_ISDIR(dir->mode) || !linkname)
		return NULL;

	/* check if entry exists */
	name_len = strlen(name);
	if (devfs_find_entry(dir, name, name_len))
		return NULL;

	/* allocate new entry */
	de = devfs_alloc_entry(name, name_len, S_IFLNK | 0555);
	if (!de)
		return NULL;

	/* set linkname */
	de->u.symlink.linkname_len = strlen(linkname);
	de->u.symlink.linkname = strdup(linkname);
	if (!de->u.symlink.linkname) {
		kfree(de);
		return NULL;
	}

	/* add entry to parent */
	de->parent = dir;
	list_add_tail(&de->list, &dir->u.dir.entries);

	return de;
}