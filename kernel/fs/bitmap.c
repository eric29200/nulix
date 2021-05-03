#include <fs/fs.h>
#include <fs/buffer.h>
#include <proc/sched.h>
#include <mm/mm.h>
#include <string.h>

/* root super block */
extern struct minix_super_block_t *root_sb;

/*
 * Get first free bit in a bitmap block (inode or block).
 */
static int get_free_bitmap(char *map)
{
  int i, j;

  for (i = 0; i < BLOCK_SIZE; i++)
    for (j = 0; j < 8; j++)
      if (!(map[i] & (0x1 << j)))
        return i * 8 + j;

  return -1;
}

/*
 * Set bit in a bitmap block (inode or block).
 */
static void set_bitmap(char *map, int i)
{
  map[i / 8] |= (0x1 << (i % 8));
}

/*
 * Create a new inode.
 */
struct inode_t *new_inode()
{
  struct inode_t *inode;
  int i, j;

  /* find first free inode in bitmap */
  for (i = 0; i < root_sb->s_imap_blocks; i++) {
    j = get_free_bitmap(root_sb->s_imap[i]);
    if (j != -1)
      break;
  }

  /* no free inode */
  if (j == -1)
    return NULL;

  /* set inode in bitmap */
  set_bitmap(root_sb->s_imap[i], j);

  /* allocate a new inode */
  inode = (struct inode_t *) kmalloc(sizeof(struct inode_t));
  if (!inode)
    return NULL;

  /* set inode */
  memset(inode, 0, sizeof(struct inode_t));
  inode->i_time = CURRENT_TIME;
  inode->i_nlinks = 1;
  inode->i_ino = i * BLOCK_SIZE * 8 + j;
  inode->i_ref = 1;
  inode->i_sb = root_sb;
  inode->i_dev = root_sb->s_dev;

  return inode;
}
