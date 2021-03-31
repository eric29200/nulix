#include <kernel/kfs.h>
#include <lib/stdio.h>

/* kfs superblock */
struct kfs_superblock_t *kfs_sb;

/*
 * Mount a kfs file system.
 */
int kfs_mount(uint32_t location)
{
  kfs_sb = (struct kfs_superblock_t *) location;
  if (kfs_sb->s_magic != KFS_MAGIC) {
    kfs_sb = NULL;
    return -1;
  }

  return 0;
}

int kfd_open(const char *path)
{
  return 0;
}

int kfs_close(int fd)
{
  return 0;
}

size_t kfs_read(int fd, char *buffer, size_t len, off_t offset)
{
  return 0;
}

size_t kfs_write(int fd, const char *buffer, size_t len, off_t offset)
{
  return 0;
}
