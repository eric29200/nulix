#include <kernel/kfs.h>
#include <lib/stdio.h>

struct kfs_superblock_t *test(uint32_t location)
{
  struct kfs_superblock_t *super_block;

  super_block = (struct kfs_superblock_t *) location;
  if (super_block->s_magic == KFS_MAGIC) {
    printf("ok\n");
  }

  return super_block;
}
