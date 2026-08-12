#include <fs/fs.h>
#include <fs/cramfs_fs.h>
#include <stdio.h>
#include <stderr.h>
#include <fcntl.h>

#define READ_BUFFERS		2
#define BLKS_PER_BUF_SHIFT	2
#define BLKS_PER_BUF		(1 << BLKS_PER_BUF_SHIFT)
#define BUFFER_SIZE		(BLKS_PER_BUF * PAGE_SIZE)

/* cramfs block cache (store 2 contiguous blocks) */
static char read_buffers[READ_BUFFERS][BUFFER_SIZE] = { 0 };
static unsigned buffer_blocknr[READ_BUFFERS] = { 0 };
static struct super_block * buffer_dev[READ_BUFFERS] = { 0 };
static int next_buffer = 0;

/*
 * Read cramfs data (may read 2 contiguous blocks).
 */
void *cramfs_read(struct super_block *sb, uint32_t offset, size_t len)
{
	struct buffer_head * bh_array[BLKS_PER_BUF];
	uint32_t blocknr, blk_offset;
	int i, buffer;
	char *data;

	/* check length */
	if (!len)
		return NULL;

	/* compute block */
	blocknr = offset >> PAGE_SHIFT;
	offset &= PAGE_SIZE - 1;

	/* check if an existing buffer already has the data */
	for (i = 0; i < READ_BUFFERS; i++) {
		if (buffer_dev[i] != sb)
			continue;
		if (blocknr < buffer_blocknr[i])
			continue;

		blk_offset = (blocknr - buffer_blocknr[i]) << PAGE_SHIFT;
		blk_offset += offset;
		if (blk_offset + len > BUFFER_SIZE)
			continue;

		return read_buffers[i] + blk_offset;
	}

	/* read pages if needed */
	for (i = 0; i < BLKS_PER_BUF; i++)
		bh_array[i] = bread(sb->s_dev, blocknr + i, PAGE_SIZE);

	/* and save them */
	buffer = next_buffer;
	buffer_blocknr[buffer] = blocknr;
	buffer_dev[buffer] = sb;
	next_buffer = next_buffer + 1 >= READ_BUFFERS ? 0 : next_buffer + 1;

	for (i = 0, data = read_buffers[buffer]; i < BLKS_PER_BUF; i++, data += PAGE_SIZE) {
		if (bh_array[i]) {
			memcpy(data, bh_array[i]->b_data, PAGE_SIZE);
			brelse(bh_array[i]);
		} else {
			memset(data, 0, PAGE_SIZE);
		}
	}

	return read_buffers[buffer] + offset;
}

/*
 * Read super block.
 */
static struct super_block *cramfs_read_super(struct super_block *sb, const char *dev_name, void *data, int silent)
{
	struct cramfs_super_block *csb;
	struct buffer_head *sbh;
	uint32_t root_offset;

	/* unused data */
	UNUSED(dev_name);
	UNUSED(data);

	/* set default block size */
	set_blocksize(sb->s_dev, PAGE_SIZE);
	sb->s_blocksize = PAGE_SIZE;
	sb->s_blocksize_bits = PAGE_SHIFT;

	/* read super block */
	sbh = bread(sb->s_dev, 0, sb->s_blocksize);
	if (!sbh)
		goto err_bad_sb;

	/* check magic number */
	csb = (struct cramfs_super_block *) sbh->b_data;
	if (csb->magic != CRAMFS_MAGIC)
		goto err_bad_magic;

	/* check signature */
	if (memcmp(csb->signature, CRAMFS_SIGNATURE, sizeof(csb->signature)))
		goto err_bad_signature;

	/* check flags */
	if (csb->flags & ~CRAMFS_SUPPORTED_FLAGS)
		goto err_features;

	/* root inode must be a directory */
	if (!S_ISDIR(csb->root.mode))
		goto err_root_mode;

	/* check root offset */
	root_offset = csb->root.offset << 2;
	if (root_offset == 0)
		printf("cramfs: empty filesystem");
	else if (root_offset != sizeof(struct cramfs_super_block))
		goto err_root_offset;

	/* set super block */
	sb->s_magic = csb->magic;
	sb->s_op = &cramfs_sops;

	/* get root inode */
	sb->s_root = d_alloc_root(cramfs_get_inode(sb, &csb->root, 0));
	if (!sb->s_root)
		goto err_root_inode;

	/* release super block buffer */
	brelse(sbh);
	return sb;
err_root_inode:
	if (!silent)
		printf("[Cramfs] Can't read root inode\n");
	goto err_release_sb;
err_root_offset:
	if (!silent)
		printf("[Cramfs] Bad root offset %lu\n", root_offset);
	goto err_release_sb;
err_root_mode:
	if (!silent)
		printf("[Cramfs] Root inode is not a directory\n");
	goto err_release_sb;
err_features:
	if (!silent)
		printf("[Cramfs] Unsupported filesystem features\n");
	goto err_release_sb;
err_bad_signature:
	if (!silent)
		printf("[Cramfs] Bad signature\n");
	goto err_release_sb;
err_bad_magic:
	if (!silent)
		printf("[Cramfs] Bad magic number\n");
err_release_sb:
	brelse(sbh);
	goto err;
err_bad_sb:
	if (!silent)
		printf("[Minix-fs] Can't read super block\n");
err:
	sb->s_dev = 0;
	return NULL;
}

/*
 * Get statistics on file system.
 */
static int cramfs_statfs(struct super_block *sb, struct statfs64 *buf)
{
	memset(buf, 0, sizeof(struct statfs64));
	buf->f_type = sb->s_magic;
	buf->f_bsize = sb->s_blocksize;
	buf->f_bfree = 0;
	buf->f_bavail = 0;
	buf->f_ffree = 0;
	buf->f_namelen = 255;

	return 0;
}

/*
 * Cramfs super operations.
 */
struct super_operations cramfs_sops = {
	.statfs			= cramfs_statfs,
};

/*
 * Cramfs file system.
 */
static struct file_system_type cramfs_fs = {
	.name			= "cramfs",
	.flags			= FS_REQUIRES_DEV,
	.read_super		= cramfs_read_super,
};
/*
 * Init cramfs file system.
 */
int init_cramfs_fs()
{
	return register_filesystem(&cramfs_fs);
}
