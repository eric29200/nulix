#include <fs/fs.h>
#include <proc/sched.h>
#include <proc/timer.h>
#include <drivers/ata.h>
#include <mm/mm.h>
#include <string.h>
#include <stderr.h>
#include <stdio.h>
#include <time.h>

/* global buffer table */
static struct buffer_head_t *buffer_table = NULL;
static LIST_HEAD(lru_buffers);

/* sync timer */
static struct timer_event_t bsync_tm;

/*
 * Write a block buffer.
 */
static int bwrite(struct buffer_head_t *bh)
{
  int ret;

  if (!bh)
    return -EINVAL;

  /* write to block device */
  ret = ata_write(bh->b_dev, bh);
  if (ret)
    return ret;

  bh->b_dirt = 0;
  return ret;
}

/*
 * Get an empty buffer.
 */
static struct buffer_head_t *get_empty_buffer()
{
  struct buffer_head_t *bh = NULL, *bh_tmp;
  struct list_head_t *pos;

  /* get first free entry from LRU list */
  list_for_each(pos, &lru_buffers) {
    bh_tmp = list_entry(pos, struct buffer_head_t, b_list);
    if (!bh_tmp->b_ref) {
      bh = bh_tmp;
      break;
    }
  }

  /* no free buffer : exit */
  if (!bh)
    return NULL;

  /* write it on disk if needed */
  if (bh->b_dirt && bwrite(bh))
    printf("Can't write block %d on disk\n", bh->b_blocknr);

  /* remove it from list */
  list_del(&bh->b_list);

  /* reset buffer */
  memset(bh, 0, sizeof(struct buffer_head_t));
  bh->b_ref = 1;

  /* add it at the end of the list */
  list_add_tail(&bh->b_list, &lru_buffers);

  return bh;
}

/*
 * Get a buffer (from cache or create one).
 */
struct buffer_head_t *getblk(dev_t dev, uint32_t block)
{
  struct buffer_head_t *bh;
  int i;

  /* try to find buffer in cache */
  for (i = 0; i < NR_BUFFER; i++) {
    if (buffer_table[i].b_blocknr == block) {
      buffer_table[i].b_ref++;

      /* put it in front of the list */
      list_del(&buffer_table[i].b_list);
      list_add_tail(&buffer_table[i].b_list, &lru_buffers);

      return &buffer_table[i];
    }
  }

  /* get an empty buffer */
  bh = get_empty_buffer();
  if (!bh)
    return NULL;

  /* set buffer */
  bh->b_dev = dev;
  bh->b_blocknr = block;
  bh->b_uptodate = 0;

  return bh;
}

/*
 * Read a block from a device.
 */
struct buffer_head_t *bread(dev_t dev, uint32_t block)
{
  struct buffer_head_t *bh;

  /* get buffer */
  bh = getblk(dev, block);
  if (!bh)
    return NULL;

  /* read it from device */
  if (!bh->b_uptodate && ata_read(dev, bh) != 0) {
    brelse(bh);
    return NULL;
  }

  bh->b_uptodate = 1;
  return bh;
}

/*
 * Release a buffer.
 */
void brelse(struct buffer_head_t *bh)
{
  if (!bh)
    return;

  /* update inode reference count */
  bh->b_ref--;
}

/*
 * Write all dirty buffers on disk.
 */
void bsync()
{
  int i;

  /* write all dirty buffers */
  for (i = 0; i < NR_BUFFER; i++) {
    if (buffer_table[i].b_dirt && bwrite(&buffer_table[i])) {
      printf("Can't write block %d on disk\n", buffer_table[i].b_blocknr);
      panic("Disk error");
    }
  }
}

/*
 * Bsync timer handler.
 */
static void bsync_timer_handler()
{
  /* sync all buffers */
  bsync();

  /* reschedule timer */
  timer_event_mod(&bsync_tm, jiffies + ms_to_jiffies(BSYNC_TIMER_MS));
}

/*
 * Init buffers.
 */
int binit()
{
  int i;

  /* allocate buffers */
  buffer_table = (struct buffer_head_t *) kmalloc(sizeof(struct buffer_head_t) * NR_BUFFER);
  if (!buffer_table)
    return -ENOMEM;

  /* memzero all buffers */
  memset(buffer_table, 0, sizeof(struct buffer_head_t) * NR_BUFFER);

  /* init Last Recently Used buffers list */
  INIT_LIST_HEAD(&lru_buffers);

  /* add all buffers to LRU list */
  for (i = 0; i < NR_BUFFER; i++)
    list_add(&buffer_table[i].b_list, &lru_buffers);

  /* create sync timer */
  timer_event_init(&bsync_tm, bsync_timer_handler, NULL, jiffies + ms_to_jiffies(BSYNC_TIMER_MS));
  timer_event_add(&bsync_tm);

  return 0;
}
