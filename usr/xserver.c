#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <poll.h>
#include <fcntl.h>
#include <errno.h>

#define FRAME_BUFFER_WITDH        1024
#define FRAME_BUFFER_HEIGHT       768

/* global frame buffer */
static uint32_t *frame_buffer;

/*
 * Mouse event.
 */
struct mouse_event_t {
  int32_t       x;
  int32_t       y;
  unsigned char buttons;
  uint32_t      state;
};

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


/* bitmaps */
static struct bitmap_t bmp_wallpaper;
static struct bitmap_t bmp_cursor;

/* mouse position */
static int32_t mouse_x = 0;
static int32_t mouse_y = 0;

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
 * Draw a RGB bitmap on frame buffer.
 */
static void draw_bitmap_rgb(struct bitmap_t *bmp, int32_t x, int32_t y)
{
  uint32_t *frame_buffer_row, i, j, k, r, g, b;
  char *bmp_row;

  for (i = 0; i < bmp->height; i++) {
    /* check overflow */
    if (y + i + 1 >= FRAME_BUFFER_HEIGHT)
      continue;

    /* get frame buffer row */
    frame_buffer_row = (void *) frame_buffer + (FRAME_BUFFER_HEIGHT - y - i - 1) * FRAME_BUFFER_WITDH * 4;

    /* get bmp row */
    bmp_row = bmp->image_bytes + i * bmp->width * 3;

    /* set frame buffer row */
    for (k = 0, j = 0; k < bmp->width; k++) {
      /* check overflow */
      if (x + k >= FRAME_BUFFER_WITDH)
        continue;

      b = bmp_row[j++] & 0xFF;
      g = bmp_row[j++] & 0xFF;
      r = bmp_row[j++] & 0xFF;
      frame_buffer_row[x + k] = (((r << 16) | (g << 8) | (b)) & 0x00FFFFFF) | 0xFF000000;
    }
  }
}

/*
 * Draw a RGBA bitmap on frame buffer.
 */
static void draw_bitmap_rgba(struct bitmap_t *bmp, int32_t x, int32_t y)
{
  uint32_t *frame_buffer_row, i, j, k, r, g, b, a;
  char *bmp_row;

  for (i = 0; i < bmp->height; i++) {
    /* check overflow */
    if (y + i + 1 >= FRAME_BUFFER_HEIGHT)
      continue;

    /* get frame buffer row */
    frame_buffer_row = (void *) frame_buffer + (FRAME_BUFFER_HEIGHT - y - i - 1) * FRAME_BUFFER_WITDH * 4;

    /* get bmp row */
    bmp_row = bmp->image_bytes + i * bmp->width * 4;

    /* set frame buffer row */
    for (k = 0, j = 0; k < bmp->width; k++) {
      /* check overflow */
      if (x + k >= FRAME_BUFFER_WITDH)
        continue;

      b = bmp_row[j++] & 0xFF;
      g = bmp_row[j++] & 0xFF;
      r = bmp_row[j++] & 0xFF;
      a = bmp_row[j++] & 0xFF;
      frame_buffer_row[x + k] = (a << 24) | (r << 16) | (g << 8) | b;
    }
  }
}

/*
 * Set wallpaper.
 */
static int draw_frame_buffer(int fd_fb)
{
  int ret;

  /* fill in frame buffer with wallpaper */
  draw_bitmap_rgb(&bmp_wallpaper, 0, 0);

  /* draw cursor */
  draw_bitmap_rgba(&bmp_cursor, mouse_x, mouse_y);

  /* seek to start of frame buffer */
  ret = lseek(fd_fb, 0, SEEK_SET);
  if (ret != 0)
    goto out;

  /* write frame buffer */
  write(fd_fb, frame_buffer, FRAME_BUFFER_WITDH * FRAME_BUFFER_HEIGHT * 4);

out:
  return ret;
}

/*
 * Wait for mouse event.
 */
static void wait_for_mouse_event(int fd_mouse)
{
  struct mouse_event_t mouse_event;
  struct pollfd pfds[1];

  /* wait for mouse event */
  pfds[0].fd = fd_mouse;
  pfds[0].events = POLLIN;
  poll(pfds, 1, -1);

  /* read mouse event */
  read(fd_mouse, &mouse_event, sizeof(struct mouse_event_t));

  /* update mouse x */
  mouse_x += mouse_event.x;
  if (mouse_x > FRAME_BUFFER_WITDH)
    mouse_x = FRAME_BUFFER_WITDH;
  else if (mouse_x < 0)
    mouse_x = 0;

  /* update mouse y */
  mouse_y += mouse_event.y;
  if (mouse_y > FRAME_BUFFER_HEIGHT)
    mouse_y = FRAME_BUFFER_HEIGHT;
  else if (mouse_y < 0)
    mouse_y = 0;
}

/*
 * Main server loop (wait for events).
 */
static int main_loop(int fd_fb, int fd_mouse)
{
  int ret = 0;

  for (;;) {
    /* wait for mouse event */
    wait_for_mouse_event(fd_mouse);

    /* draw frame buffer */
    draw_frame_buffer(fd_fb);
  }

  return ret;
}

/*
 * Load a wallpaper.
 */
int main()
{
  int fd_fb, fd_mouse, ret = 0;

  /* create frame buffer */
  frame_buffer = (uint32_t *) malloc(FRAME_BUFFER_WITDH * FRAME_BUFFER_HEIGHT * 4);
  if (!frame_buffer)
    return ENOMEM;

  /* load cursor */
  ret = bitmap_load("/etc/cursor.bmp", &bmp_cursor);
  if (ret)
    return ret;

  /* load wallpaper */
  ret = bitmap_load("/etc/wallpaper.bmp", &bmp_wallpaper);
  if (ret)
    return ret;

  /* open frame bufer */
  fd_fb = open("/dev/fb", O_RDWR);
  if (fd_fb < 0)
    return fd_fb;

  /* open mouse */
  fd_mouse = open("/dev/mouse", O_RDONLY);
  if (fd_mouse < fd_fb) {
    close(fd_fb);
    return fd_mouse;
  }

  /* draw frame buffer */
  draw_frame_buffer(fd_fb);

  /* main loop */
  ret = main_loop(fd_fb, fd_mouse);

  /* close frame buffer */
  close(fd_fb);
  close(fd_mouse);

  return ret;
}
