#include <fs/fs.h>
#include <proc/sched.h>
#include <proc/timer.h>
#include <drivers/block/ata.h>
#include <drivers/block/blk_dev.h>
#include <mm/mm.h>
#include <mm/highmem.h>
#include <x86/bitops.h>
#include <string.h>
#include <stderr.h>
#include <stdio.h>
#include <time.h>
#include <dev.h>

#define NR_BUFFERS_MAX			65521
#define NR_SIZES			4
#define BUFSIZE_INDEX(size)		(buffersize_index[(size) >> 9])
#define NBUF				64
#define MAX_BUF_PER_PAGE		(PAGE_SIZE / 512)
#define NR_RESERVED			(4 * (MAX_BUF_PER_PAGE))
#define MAX_UNUSED_BUFFERS		((NR_RESERVED) + 20)

#define HASH_BITS			12
#define HASH_SIZE			(1 << HASH_BITS)

/* global buffer table */
static int nr_buffers = 0;
static int nr_buffer_heads = 0;
static int nr_unused_buffer_heads = 0;
uint32_t buffermem_pages = 0;
static struct buffer_head *buffer_hash_table[HASH_SIZE];

/* buffers lists */
static char buffersize_index[9] = { -1, 0, 1, -1, 2, -1, -1, -1, 3 };
static LIST_HEAD(unused_list);
static LIST_HEAD(used_list);
static LIST_HEAD(dirty_list);
static struct list_head free_list[NR_SIZES];

/*
 * Wait on a buffer.
 */
void wait_on_buffer(struct buffer_head *bh)
{
	if (!buffer_locked(bh))
		return;

	execute_block_requests();

	if (buffer_locked(bh))
		panic("wait_on_buffer: buffer still locked after execute_block_requests()");
}

/*
 * Wait on buffers.
 */
void wait_on_buffers(size_t bhs_count, struct buffer_head **bhs)
{
	size_t i;

	for (i = 0; i < bhs_count; i++)
		wait_on_buffer(bhs[i]);
}

/*
 * Lock a buffer.
 */
void lock_buffer(struct buffer_head *bh)
{
	set_bit(&bh->b_state, BH_Lock);
}

/*
 * Unlock a buffer.
 */
void unlock_buffer(struct buffer_head *bh)
{
	clear_bit(&bh->b_state, BH_Lock);
}

/*
 * Mark a buffer clean.
 */
void mark_buffer_clean(struct buffer_head *bh)
{
	if (buffer_dirty(bh)) {
		clear_bit(&bh->b_state, BH_Dirty);
		list_del(&bh->b_list);
		list_add(&bh->b_list, &used_list);
	}
}

/*
 * Mark a buffer dirty.
 */
void mark_buffer_dirty(struct buffer_head *bh)
{
	struct buffer_head *next = NULL;
	struct list_head *pos;

	if (!buffer_dirty(bh)) {
		set_bit(&bh->b_state, BH_Dirty);
		list_del(&bh->b_list);

		/* keep dirty list sorted */
		list_for_each(pos, &dirty_list) {
			next = list_entry(pos, struct buffer_head, b_list);

			if (next->b_dev > bh->b_dev)
				break;
			if (next->b_block == bh->b_block && next->b_block > bh->b_block)
				break;
		}

		if (next)
			list_add_tail(&bh->b_list, &next->b_list);
		else
			list_add(&bh->b_list, &dirty_list);
	}
}

/*
 * Mark a buffer up to date.
 */
void mark_buffer_uptodate(struct buffer_head *bh, int on)
{
	if (on)
	 	set_bit(&bh->b_state, BH_Uptodate);
	else
	 	clear_bit(&bh->b_state, BH_Uptodate);
}

/*
 * Mark a buffer new.
 */
void mark_buffer_new(struct buffer_head *bh)
{
	set_bit(&bh->b_state, BH_New);
}

/*
 * Hash a buffer.
 */
static inline int __buffer_hashfn(dev_t dev, uint32_t block)
{
	return (dev ^ block) % HASH_SIZE;
}

/*
 * Get hash bucket.
 */
static inline struct buffer_head **buffer_hash(dev_t dev, uint32_t block)
{
	return buffer_hash_table + __buffer_hashfn(dev, block);
}

/*
 * Insert a buffer in hash table.
 */
static void add_to_buffer_cache(struct buffer_head *bh)
{
	struct buffer_head **b;

	b = buffer_hash(bh->b_dev, bh->b_block);
	bh->b_prev_hash = NULL;
	bh->b_next_hash = *b;

	if (*b)
		bh->b_next_hash->b_prev_hash = bh;

	/* update head */
	*b = bh;
}

/*
 * Remove a buffer from cache.
 */
static void remove_from_buffer_cache(struct buffer_head *bh)
{
	struct buffer_head *next_hash = bh->b_next_hash, *prev_hash = bh->b_prev_hash;
	struct buffer_head **b;

	/* unlink this buffer */
	bh->b_next_hash = NULL;
	bh->b_prev_hash = NULL;

	/* update next */
	if (next_hash)
		next_hash->b_prev_hash = prev_hash;

	/* update previous */
	if (prev_hash)
		prev_hash->b_next_hash = next_hash;

	/* update head */
	b = buffer_hash(bh->b_dev, bh->b_block);
	if (*b == bh)
		*b = next_hash;
}

/*
 * End of IO.
 */
static void end_buffer_io_sync(struct buffer_head *bh, int uptodate)
{
	/* mark buffer up to date */
	mark_buffer_clean(bh);
	mark_buffer_uptodate(bh, uptodate);
	unlock_buffer(bh);
	bh->b_count--;

	/* remove it from request */
	list_del(&bh->b_list_req);
}

/*
 * End of IO.
 */
static void end_buffer_io_async(struct buffer_head *bh, int uptodate)
{
	struct page *page = bh->b_page;
	struct buffer_head *tmp;
	int fullup = 1;

	/* mark buffer up to date */
	mark_buffer_clean(bh);
	mark_buffer_uptodate(bh, uptodate);
	unlock_buffer(bh);
	bh->b_count--;

	/* remove it from request */
	list_del(&bh->b_list_req);

	/* check buffer of this page */
	tmp = bh;
	do {
		/* buffer stil used */
		if (tmp->b_count)
			return;

		/* buffer/page not up to date */
		if (!buffer_uptodate(bh))
			fullup = 0;

		tmp = tmp->b_this_page;
	} while (tmp != bh);

	/* mark page up to date */
	if (fullup)
		SetPageUptodate(page);

	/* free buffers */
	UnlockPage(page);
	kunmap(bh->b_page);
	free_async_buffers(bh);
}

/*
 * Put a buffer in unused list.
 */
static void put_unused_buffer_head(struct buffer_head *bh)
{
	/* free buffer */
	if (nr_unused_buffer_heads >= MAX_UNUSED_BUFFERS) {
		nr_buffer_heads--;
		list_del(&bh->b_list);
		kfree(bh);
		return;
	}

	/* or add it to unused list */
	list_del(&bh->b_list);
	memset(bh, 0, sizeof(struct buffer_head));
	list_add(&bh->b_list, &unused_list);
	nr_unused_buffer_heads++;
}

/*
 * Get an unused buffer.
 */
static struct buffer_head *get_unused_buffer_head(int async)
{
	struct buffer_head *bh;

	/* try to reuse a buffer */
	if (nr_unused_buffer_heads > NR_RESERVED || (async && nr_unused_buffer_heads > 0)) {
		bh = list_first_entry(&unused_list, struct buffer_head, b_list);
		list_del(&bh->b_list);
		list_add(&bh->b_list, &used_list);
		nr_unused_buffer_heads--;
		return bh;
	}

	/* allocate a new buffer */
	bh = (struct buffer_head *) kmalloc(sizeof(struct buffer_head));
	if (!bh)
		return NULL;

	/* set new buffer */
	memset(bh, 0, sizeof(struct buffer_head));
	list_add(&bh->b_list, &used_list);
	nr_buffer_heads++;

	return bh;
}

/*
 * Create new buffers.
 */
static struct buffer_head *create_buffers(struct page *page, dev_t dev, size_t blocksize, int async)
{
	struct buffer_head *bh, *head, *tail;
	int offset;

	/* create buffers */
	head = NULL;
	tail = NULL;
	for (offset = PAGE_SIZE - blocksize; offset >= 0; offset -= blocksize) {
		/* get an unused buffer */
		bh = get_unused_buffer_head(async);
		if (!bh)
			goto err;

		/* set buffer */
		bh->b_dev = dev;
		bh->b_data = page_address(page) + offset;
		bh->b_size = blocksize;
		bh->b_this_page = head;
		bh->b_page = page;
		bh->b_end_io = end_buffer_io_sync;

		/* set tail and head */
		if (!head)
			tail = bh;
		head = bh;
	}

	/* end circular list */
	if (tail)
		tail->b_this_page = head;

	/* set buffers */
	page->buffers = head;

	return head;
err:
	/* put buffers in unused list */
	for (bh = head; bh != NULL;) {
		head = bh;
		bh = bh->b_this_page;
		put_unused_buffer_head(head);
	}

	return NULL;
}

/*
 * Grow buffers list.
 */
static int grow_buffers(size_t size)
{
	size_t isize = BUFSIZE_INDEX(size);
	struct buffer_head *bh, *tmp;
	struct page *page;

	/* get a page */
	page = __get_free_page(GFP_KERNEL);
	if (!page)
		return -ENOMEM;

	/* create buffers */
	if (!page->buffers) {
		if (!create_buffers(page, 0, size, 0)) {
			__free_page(page);
			return -ENOMEM;
		}
	}
	bh = page->buffers;

	/* add new buffers to free list */
	tmp = bh;
	do {
		tmp->b_page = page;
		list_del(&tmp->b_list);
		list_add_tail(&tmp->b_list, &free_list[isize]);
		nr_buffers++;
		tmp = tmp->b_this_page;
	} while (tmp != bh);

	/* set page buffers */
	page->buffers = bh;
	buffermem_pages++;

	return 0;
}

/*
 * Find a buffer in hash table.
 */
struct buffer_head *find_buffer(dev_t dev, uint32_t block, size_t blocksize)
{
	struct buffer_head *bh;

	for (bh = *buffer_hash(dev, block); bh != NULL; bh = bh->b_next_hash) {
		if (bh->b_block == block && bh->b_dev == dev && bh->b_size == blocksize) {
			bh->b_count++;
			return bh;
		}
	}

	return NULL;
}

/*
 * Get a buffer (from cache or create one).
 */
struct buffer_head *getblk(dev_t dev, uint32_t block, size_t blocksize)
{
	size_t isize = BUFSIZE_INDEX(blocksize);
	struct buffer_head *bh;

	/* try to find buffer in cache */
	bh = find_buffer(dev, block, blocksize);
	if (bh)
		return bh;

	/* refill free list if needed */
	if (list_empty(&free_list[isize])) {
		grow_buffers(blocksize);

		/* recheck free list */
		if (list_empty(&free_list[isize]))
			return NULL;
	}

	/* get first free buffer */
	bh = list_first_entry(&free_list[isize], struct buffer_head, b_list);
	list_del(&bh->b_list);
	list_add(&bh->b_list, &used_list);

	/* set buffer */
	bh->b_count = 1;
	bh->b_dev = dev;
	bh->b_block = block;
	mark_buffer_uptodate(bh, 0);

	/* cache the new buffer */
	add_to_buffer_cache(bh);

	return bh;
}

/*
 * Read a block from a device.
 */
struct buffer_head *bread(dev_t dev, uint32_t block, size_t blocksize)
{
	struct buffer_head *bh;

	/* get buffer */
	bh = getblk(dev, block, blocksize);
	if (!bh)
		return NULL;

	/* buffer up to date */
	if (buffer_uptodate(bh))
		return bh;

	/* read it from device */
	ll_rw_block(READ, 1, &bh);
	wait_on_buffer(bh);

	return bh;
}

/*
 * Release a buffer.
 */
void brelse(struct buffer_head *bh)
{
	if (!bh)
		return;

	/* update buffer reference count */
	bh->b_count--;
}

/*
 * Try to free a buffer.
 */
int try_to_free_buffers(struct page *page)
{
	struct buffer_head *tmp, *tmp1;

	/* check if all page buffers can be freed */
	tmp = page->buffers;
	do {
		/* used buffer */
		if (tmp->b_count || buffer_dirty(tmp))
			return 0;

		/* go to next buffer in page */
		tmp = tmp->b_this_page;
	} while (tmp != page->buffers);

	/* ok to free buffers */
	tmp = page->buffers;
	do {
		/* save next buffer */
		tmp1 = tmp->b_this_page;

		/* remove it from lists */
		remove_from_buffer_cache(tmp);
		put_unused_buffer_head(tmp);
		nr_buffers--;

		/* go to next buffer in page */
		tmp = tmp1;
	} while (tmp != page->buffers);

	/* free page */
	buffermem_pages--;
	page->buffers = NULL;
	__free_page(page);

	return 1;
}

/*
 * Write all dirty buffers on disk.
 */
static void sync_buffers(dev_t dev)
{
	struct buffer_head *bh, *bhs_list[NBUF];
	struct list_head *pos, *n;
	size_t bhs_count = 0;

	list_for_each_safe(pos, n, &dirty_list) {
		bh = (struct buffer_head *) list_entry(pos, struct buffer_head, b_list);

		/* clean buffer */
		if (!buffer_dirty(bh) || (dev && bh->b_dev != dev))
			continue;

		/* write buffers */
		if (bhs_count == NBUF) {
			ll_rw_block(WRITE, bhs_count, bhs_list);
			wait_on_buffers(bhs_count, bhs_list);
			bhs_count = 0;
		}

		/* add buffer */
		bhs_list[bhs_count++] = bh;
	}

	/* write last buffers */
	if (bhs_count) {
		ll_rw_block(WRITE, bhs_count, bhs_list);
		wait_on_buffers(bhs_count, bhs_list);
	}
}

/*
 * Synchronize device on disk.
 */
void sync_dev(dev_t dev)
{
	sync_buffers(dev);
	sync_inodes(dev);
}

/*
 * Sync system call.
 */
int sys_sync()
{
	sync_dev(0);
	return 0;
}

/*
 * Set blocksize of a device.
 */
void set_blocksize(dev_t dev, size_t blocksize)
{
	/* check device */
	if (!blksize_size[major(dev)])
		return;

	/* check block size */
	switch (blocksize) {
		case 512:
		case 1024:
		case 2048:
		case 4096:
			break;
		default:
			panic("set_blocksize : invalid block size");
	}

	/* nothing to do */
	if (blksize_size[major(dev)][minor(dev)] == blocksize)
		return;

 	/* sync buffers */
	sync_buffers(dev);

	/* set block size */
	blksize_size[major(dev)][minor(dev)] = blocksize;
}

/*
 * Fsync system call (don't do anything because all block buffers should be clean).
 */
int sys_fsync(int fd)
{
	UNUSED(fd);
	return 0;
}

/*
 * Generic block map.
 */
int generic_block_bmap(struct inode *inode, uint32_t block)
{
	struct buffer_head tmp = { 0 };

	if (inode->i_op->get_block(inode, block, &tmp, 0))
		return 0;

	return tmp.b_block;
}

/*
 * Free buffers.
 */
void free_async_buffers(struct buffer_head *bh)
{
	struct buffer_head *tmp = bh, *next;
	struct page *page = bh->b_page;

	do {
		next = tmp->b_this_page;
		put_unused_buffer_head(tmp);
		tmp = next;
	} while (tmp != bh);

	if (page)
		page->buffers = NULL;
}

/*
 * Read/write a page.
 */
int brw_page(int rw, struct page *page, dev_t dev, uint32_t *blocks, size_t nr_blocks, size_t size)
{
	struct buffer_head *bh, *next, *tmp, *bhs_list[MAX_BUF_PER_PAGE];
	size_t bhs_count = 0, i;

	/* create buffers */
	if (!page->buffers) {
		if (!create_buffers(page, dev, size, 1)) {
			UnlockPage(page);
			return -ENOMEM;
		}
	}
	bh = page->buffers;

	/* read/write block by block */
	for (i = 0, next = bh; i < nr_blocks; i++, next = next->b_this_page) {
		/* set block buffer */
		next->b_block = *(blocks++);
		next->b_end_io = end_buffer_io_async;

		/* check if buffer is already hashed */
		tmp = find_buffer(dev, next->b_block, size);
		if (tmp) {
			/* read it from disk if needed */
			if (!buffer_uptodate(tmp)) {
				if (rw == READ)
					ll_rw_block(READ, 1, &tmp);
				wait_on_buffer(tmp);
			}

			/* copy data */
			if (rw == READ) {
				memcpy(next->b_data, tmp->b_data, size);
			} else {
				memcpy(tmp->b_data, next->b_data, size);
				mark_buffer_dirty(tmp);
			}

			/* release buffer */
			brelse(tmp);
			continue;
		}

		/* set buffer state */
		if (rw == READ)
			mark_buffer_uptodate(bh, 0);
		else
			mark_buffer_dirty(next);

		/* add buffer to read/write */
		bhs_list[bhs_count++] = next;
	}

	/* read/write buffers on disk or destroy buffers */
	if (bhs_count) {
		ll_rw_block(rw, bhs_count, bhs_list);
	} else {
		UnlockPage(page);
		free_async_buffers(bh);
		kunmap(page);
		SetPageUptodate(page);
	}

	return 0;
}

/*
 * Read a page.
 */
int generic_readpage(struct inode *inode, struct page *page)
{
	uint32_t block, blocks[MAX_BUF_PER_PAGE];
	size_t nr, i;

	nr = PAGE_SIZE >> inode->i_sb->s_blocksize_bits;
	block = page->offset >> inode->i_sb->s_blocksize_bits;

	/* lock page */
	LockPage(page);

	/* compute blocks */
	for (i = 0; i < nr; i++, block++)
		blocks[i] = inode->i_op->bmap(inode, block);

	/* read/write page */
	brw_page(READ, page, inode->i_dev, blocks, nr, inode->i_sb->s_blocksize);
	return 0;
}

/*
 * Prepare write = make up to date buffers.
 */
int generic_prepare_write(struct inode *inode, struct page *page, uint32_t from, uint32_t to)
{
	struct buffer_head *head, *bh, *bhs_list[MAX_BUF_PER_PAGE];
	uint32_t block, start_block, end_block;
	struct super_block *sb = inode->i_sb;
	size_t bhs_count = 0;
	int ret;

	if (!page->buffers) {
		if (!create_buffers(page, inode->i_dev, sb->s_blocksize, 1)) {
			UnlockPage(page);
			return -ENOMEM;
		}
	}
	head = page->buffers;

	/* compute first block */
	block = page->offset >> sb->s_blocksize_bits;

	/* for each block */
	for(bh = head, start_block = 0; bh != head || !start_block; block++, start_block = end_block, bh = bh->b_this_page) {
		end_block = start_block + sb->s_blocksize;

		/* block out of scope */
		if (end_block <= from)
			continue;
		if (start_block >= to)
			break;

		/* get or create block */
		ret = inode->i_op->get_block(inode, block, bh, 1);
		if (ret)
			goto err;

		/* new block = clear it */
		if (buffer_new(bh)) {
			if (PageUptodate(page)) {
				mark_buffer_uptodate(bh, 1);
				continue;
			}

			if (end_block > to)
				memset(page_address(page) + to, 0, end_block - to);
			if (start_block < from)
				memset(page_address(page) + start_block, 0, from - start_block);

			continue;
		}

		/* page up to date = block up to date */
		if (PageUptodate(page)) {
			mark_buffer_uptodate(bh, 1);
			continue; 
		}

		/* read block */
		if (!buffer_uptodate(bh) && (start_block < from || end_block > to))
			bhs_list[bhs_count++] = bh;
	}

	/* issue read command */
	if (bhs_count) {
		ll_rw_block(READ, bhs_count, bhs_list);
		wait_on_buffers(bhs_count, bhs_list);
	}

	return 0;
err:
	UnlockPage(page);
	free_async_buffers(head);
	return ret;
}

/*
 * Commit write.
 */
int generic_commit_write(struct inode *inode, struct page *page, uint32_t from, uint32_t to)
{
	struct buffer_head *head, *bh, *bhs_list[MAX_BUF_PER_PAGE];
	struct super_block *sb = inode->i_sb;
	uint32_t start_block, end_block;
	size_t bhs_count = 0;
	int partial = 0;

	/* for each block */
	for (bh = head = page->buffers, start_block = 0; bh != head || !start_block; start_block = end_block, bh = bh->b_this_page) {
		end_block = start_block + sb->s_blocksize;

		/* block out of scope */
		if (end_block <= from || start_block >= to) {
			if (!buffer_uptodate(bh))
				partial = 1;
		}

		/* block need to be written */
		bhs_list[bhs_count++] = bh;
		bh->b_end_io = end_buffer_io_async;
	}

	/* write buffers on disk or destroy buffers */
	if (bhs_count) {
		ll_rw_block(WRITE, bhs_count, bhs_list);
	} else {
		UnlockPage(page);
		free_async_buffers(bh);
		kunmap(page);
	}

	if (!partial)
		SetPageUptodate(page);

	return 0;
}

/*
 * Init buffers.
 */
void init_buffer()
{
	struct buffer_head *bh;
	int i;

	/* init buffers list */
	for (i = 0; i < NR_SIZES; i++)
		INIT_LIST_HEAD(&free_list[i]);

	/* init hash table */
	for (i = 0; i < HASH_SIZE; i++)
		buffer_hash_table[i] = NULL;

	/* create some buffers */
	for (i = 0; i < MAX_UNUSED_BUFFERS; i++) {
		bh = (struct buffer_head *) kmalloc(sizeof(struct buffer_head));
		if (!bh)
			break;

		memset(bh, 0, sizeof(struct buffer_head));
		list_add(&bh->b_list, &used_list);
		nr_buffer_heads++;

		put_unused_buffer_head(bh);
	}
}