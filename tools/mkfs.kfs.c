#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define NB_ARGS                     2
#define KFS_MAGIC                   0xA0A0A0A0
#define KFS_BLOCK_SIZE              4096
#define KFS_FILENAME_LEN            64
#define KFS_ROOT_INODE_NUMBER       1
#define KFS_SUPERBLOCK_BLOCK_NUMBER 0
#define KFS_INODESTORE_BLOCK_NUMBER 1
#define KFS_DATA_BLOCK_NUMBER       2

/*
 * Kfs super block.
 */
struct kfs_superblock_t {
  uint32_t s_magic;
  uint32_t s_block_size;
};

/*
 * Kfs inode.
 */
struct kfs_inode_t {
  uint32_t i_mode;
  uint32_t i_inode_no;
  uint32_t i_block;
  union {
    uint32_t i_size;
    uint32_t i_nb_children;
  };
};

/*
 * Kfs dir record.
 */
struct kfs_dir_record_t {
  char d_name[KFS_FILENAME_LEN];
  uint32_t d_inode_no;
};

/*
 * Usage.
 */
static void usage(const char *name)
{
  printf("Usage : %s disk_image\n", name);
}

/*
 * Write super block.
 */
static int write_superblock(int fd)
{
  struct kfs_superblock_t sb;

  /* create super block */
  sb.s_magic = KFS_MAGIC;
  sb.s_block_size = KFS_BLOCK_SIZE;

  /* go to superblock block */
  if (lseek(fd, KFS_SUPERBLOCK_BLOCK_NUMBER * KFS_BLOCK_SIZE, SEEK_SET) == -1) {
    perror("fseek");
    return -1;
  }

  /* write to disk */
  if (write(fd, &sb, sizeof(struct kfs_superblock_t)) != sizeof(struct kfs_superblock_t)) {
    fprintf(stderr, "Error writing superblock\n");
    return -1;
  }

  return 0;
}

/*
 * Write root inode.
 */
static int write_root_inode(int fd)
{
  struct kfs_inode_t root_inode;

  /* create root inode */
  root_inode.i_mode = S_IFDIR;
  root_inode.i_inode_no = KFS_ROOT_INODE_NUMBER;
  root_inode.i_block = KFS_DATA_BLOCK_NUMBER;
  root_inode.i_nb_children = 0;

  /* go to inode store block */
  if (lseek(fd, KFS_INODESTORE_BLOCK_NUMBER * KFS_BLOCK_SIZE, SEEK_SET) == -1) {
    perror("fseek");
    return -1;
  }

  /* write to disk */
  if (write(fd, &root_inode, sizeof(struct kfs_inode_t)) != sizeof(struct kfs_inode_t)) {
    fprintf(stderr, "Error writing root inode\n");
    return -1;
  }

  return 0;
}

/*
 * Create a kfs file system.
 */
int main(int argc, char **argv)
{
  int ret = 0;
  int fd;

  /* check arguments */
  if (argc != NB_ARGS) {
    usage(argv[0]);
    return -1;
  }

  /* open disk */
  fd = open(argv[1], O_WRONLY | O_TRUNC | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
  if (fd == -1) {
    perror("open");
    return -1;
  }

  /* write super block */
  ret = write_superblock(fd);
  if (ret == -1)
    goto out;

  /* write root inode */
  ret = write_root_inode(fd);
  if (ret == -1)
    goto out;

out:
  close(fd);
  return ret;
}
