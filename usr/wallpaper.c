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
 * Load a wallpaper.
 */
int main()
{
  uint32_t *frame_buffer_row, r, g, b, i, j, k;
  char *frame_buffer = NULL, *bmp_row;
  struct bitmap_t bmp;
  int ret = 0, fd_fb;

  /* load bitmap */
  ret = bitmap_load("/etc/wallpaper.bmp", &bmp);
  if (ret != 0)
    goto out;

  /* allocate frame buffer */
  frame_buffer = (char *) malloc(bmp.height * bmp.width * 4);
  if (!frame_buffer)
    goto out;

  /* create frame buffer */
  for (i = 0; i < bmp.height; i++) {
    /* get bmp and frame buffer row */
    bmp_row = bmp.image_bytes + i * bmp.width * 3;
    frame_buffer_row = (void *) frame_buffer + (bmp.height - 1 - i) * bmp.width * 4;

    /* set row */
    for (k = 0, j = 0; k < bmp.width; k++) {
      b = bmp_row[j++] & 0xFF;
      g = bmp_row[j++] & 0xFF;
      r = bmp_row[j++] & 0xFF;
      frame_buffer_row[k] = (((r << 16) | (g << 8) | (b)) & 0x00FFFFFF) | 0xFF000000;
    }
  }

  /* open frame bufer */
  fd_fb = open("/dev/fb", O_RDWR);
  if (fd_fb < 0) {
    ret = fd_fb;
    goto out;
  }

  /* write image to frame buffer */
  write(fd_fb, frame_buffer, bmp.width * bmp.height * 4);

  /* close frame buffer */
  close(fd_fb);

out:
  /* free bitmap */
  if (bmp.buf)
    free(bmp.buf);

  return ret;
}
