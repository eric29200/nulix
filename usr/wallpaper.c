#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>

/*
 * Bitmap header.
 */
struct bitmap_file_header_t {
  uint16_t    type;
  uint32_t    size;
  uint16_t    reserved1;
  uint16_t    reserved2;
  uint32_t    off_bits;
} __attribute__((packed));

/*
 * Bitmap info header.
 */
struct bitmap_info_header_t {
  uint32_t    size;
  uint32_t    width;
  uint32_t    height;
  uint16_t    planes;
  uint16_t    bit_count;
  uint32_t    compression;
  uint32_t    size_image;
  uint32_t    x_pels_per_meter;
  uint32_t    y_pels_per_meter;
  uint32_t    clr_used;
  uint32_t    clr_important;
};

/*
 * Bitmap.
 */
struct bitmap_t {
  uint32_t    width;
  uint32_t    height;
  char *      image_bytes;
  char *      buf;
  uint32_t    total_size;
  uint32_t    bpp;
};

/*
 * Load a bitmap file.
 */
static int bitmap_load(char *filename, struct bitmap_t *bmp)
{
  struct bitmap_file_header_t *header;
  struct bitmap_info_header_t *info;
  struct stat statbuf;
  int fd, ret;

  /* check parameter */
  if (!bmp)
    return -EINVAL;

  /* get input file size */
  ret = stat(filename, &statbuf);
  if (ret != 0)
    return ret;

  /* allocate buffer */
  bmp->buf = malloc(statbuf.st_size);
  if (!bmp->buf)
    return -ENOMEM;

  /* open input file */
  fd = open(filename, O_RDONLY);
  if (fd < 0) {
    free(bmp->buf);
    return fd;
  }

  /* read all file */
  if (read(fd, bmp->buf, statbuf.st_size) != statbuf.st_size) {
    free(bmp->buf);
    close(fd);
    return -EINVAL;
  }

  /* close input file */
  close(fd);

  /* parse bitmap */
  header = (struct bitmap_file_header_t *) bmp->buf;
  info = (struct bitmap_info_header_t *) (bmp->buf + sizeof(struct bitmap_file_header_t));
  bmp->width = info->width;
  bmp->height = info->height;
  bmp->image_bytes = bmp->buf + header->off_bits;
  bmp->total_size = statbuf.st_size;
  bmp->bpp = info->bit_count;

  return 0;
}

/*
 * Write a bitmap row to a frame buffer.
 */
static void bitmap_row_to_fb(struct bitmap_t *bmp, uint32_t row, int fd_fb)
{
  uint32_t frame_buffer_row[bmp->width];
  uint32_t r, g, b, i, j;
  char *bmp_row;

  /* get bit map row */
  bmp_row = bmp->image_bytes + row * bmp->width * 3;

  /* set frame buffer row */
  for (i = 0, j = 0; i < bmp->width; i++) {
    b = bmp_row[j++] & 0xFF;
    g = bmp_row[j++] & 0xFF;
    r = bmp_row[j++] & 0xFF;
    frame_buffer_row[i] = (((r << 16) | (g << 8) | (b)) & 0x00FFFFFF) | 0xFF000000;
  }

  /* write row to frame buffer */
  write(fd_fb, frame_buffer_row, sizeof(uint32_t) * bmp->width);
}

/*
 * Load a wallpaper.
 */
int main()
{
  struct bitmap_t bmp;
  int ret = 0, fd_fb;
  uint32_t i;

  /* load bitmap */
  ret = bitmap_load("/etc/wallpaper.bmp", &bmp);
  if (ret != 0)
    goto out;

  /* open frame bufer */
  fd_fb = open("/dev/fb", O_RDWR);
  if (fd_fb < 0) {
    ret = fd_fb;
    goto out;
  }

  /* seek to start of frame buffer */
  ret = lseek(fd_fb, 0, SEEK_SET);
  if (ret != 0) {
    close(fd_fb);
    goto out;
  }

  /* write frame buffer row by row */
  for (i = 0; i < bmp.height; i++)
    bitmap_row_to_fb(&bmp, i, fd_fb);

  /* close frame buffer */
  close(fd_fb);

out:
  /* free bitmap */
  if (bmp.buf)
    free(bmp.buf);

  return ret;
}
