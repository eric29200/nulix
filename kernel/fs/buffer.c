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
static struct timer_event_t bsync_tm;

/*
 * Get an empty buffer.
 */
static struct buffer_head_t *get_empty_buffer()
{
  struct buffer_head_t *bh = NULL;
  int i;

repeat:
  /* find a free clean buffer */
  for (i = 0; i < NR_BUFFER; i++) {
    if (!buffer_table[i].b_ref && !buffer_table[i].b_dirt) {
      bh = &buffer_table[i];
      break;
    }
  }

  /* no more buffer : sync all buffers */
  if (!bh) {
    bsync();
    goto repeat;
  }

  /* reset inode */
  memset(bh, 0, sizeof(struct buffer_head_t));
  bh->b_ref = 1;

  return bh;
}

/*
 * Read a block from a device.
 */
struct buffer_head_t *bread(dev_t dev, uint32_t block)
{
  struct buffer_head_t *bh;
  int i;

  /* try to find buffer in table */
  for (i = 0; i < NR_BUFFER; i++) {
    if (buffer_table[i].b_blocknr == block) {
      bh = &buffer_table[i];
      bh->b_ref++;
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
 * Write a block buffer.
 */
int bwrite(struct buffer_head_t *bh)
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
    if (buffer_table[i].b_dirt) {
      if (bwrite(&buffer_table[i])) {
        printf("Can't write block %d on disk\n", buffer_table[i].b_blocknr);
        panic("Disk error");
      }
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
  /* allocate buffers */
  buffer_table = (struct buffer_head_t *) kmalloc(sizeof(struct buffer_head_t) * NR_BUFFER);
  if (!buffer_table)
    return -ENOMEM;

  /* memzero all buffers */
  memset(buffer_table, 0, sizeof(struct buffer_head_t) * NR_BUFFER);

  /* create sync timer */
  timer_event_init(&bsync_tm, bsync_timer_handler, NULL, jiffies + ms_to_jiffies(BSYNC_TIMER_MS));
  timer_event_add(&bsync_tm);

  return 0;
}
