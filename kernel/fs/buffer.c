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
static LIST_HEAD(free_buffers);
static LIST_HEAD(cached_buffers);

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

  /* get first free buffer or remove first entry from cache */
  if (!list_empty(&free_buffers)) {
    bh = list_first_entry(&free_buffers, struct buffer_head_t, b_list);
  } else {
    list_for_each(pos, &cached_buffers) {
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
  }

  /* remove it from list */
  list_del(&bh->b_list);

  /* reset buffer */
  memset(bh, 0, sizeof(struct buffer_head_t));
  bh->b_ref = 1;

  /* add it at the end of the list */
  list_add_tail(&bh->b_list, &cached_buffers);

  return bh;
}

/*
 * Read a block from a device.
 */
struct buffer_head_t *bread(dev_t dev, uint32_t block)
{
  struct buffer_head_t *bh;
  struct list_head_t *pos;

  /* try to find buffer in cache */
  list_for_each(pos, &cached_buffers) {
    bh = list_entry(pos, struct buffer_head_t, b_list);
    if (bh->b_blocknr == block) {
      bh->b_ref++;

      /* put it in front of the list */
      list_del(&bh->b_list);
      list_add(&bh->b_list, &cached_buffers);

      return bh;
    }
  }

  /* get an empty buffer */
  bh = get_empty_buffer();
  if (!bh)
    return NULL;

  /* set buffer */
  bh->b_dev = dev;
  bh->b_blocknr = block;

  /* read from device */
  if (ata_read(dev, bh) != 0) {
    brelse(bh);
    return NULL;
  }

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
  struct buffer_head_t *bh;
  struct list_head_t *pos;

  /* write all dirty buffers */
  list_for_each(pos, &cached_buffers) {
    bh = list_entry(pos, struct buffer_head_t, b_list);
    if (bh->b_dirt && bwrite(bh)) {
      printf("Can't write block %d on disk\n", bh->b_blocknr);
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

  /* init free/cached buffers lists */
  INIT_LIST_HEAD(&free_buffers);
  INIT_LIST_HEAD(&cached_buffers);

  /* add all buffers to the free list */
  for (i = 0; i < NR_BUFFER; i++)
    list_add(&buffer_table[i].b_list, &free_buffers);

  /* create sync timer */
  timer_event_init(&bsync_tm, bsync_timer_handler, NULL, jiffies + ms_to_jiffies(BSYNC_TIMER_MS));
  timer_event_add(&bsync_tm);

  return 0;
}
