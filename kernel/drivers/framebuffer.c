#include <drivers/framebuffer.h>
#include <string.h>
#include <mm/mm.h>
#include <x86/io.h>
#include <font.h>
#include <stderr.h>

/* frame buffer update functions */
static void fb_update_text(struct framebuffer_t *fb);
static void fb_update_rgb(struct framebuffer_t *fb);

/*
 * Init the framebuffer.
 */
int init_framebuffer(struct framebuffer_t *fb, struct multiboot_tag_framebuffer *tag_fb)
{
  uint32_t fb_nb_pages, i;

  /* set frame buffer */
  fb->addr = tag_fb->common.framebuffer_addr;
  fb->type = tag_fb->common.framebuffer_type;
  fb->pitch = tag_fb->common.framebuffer_pitch;
  fb->width = tag_fb->common.framebuffer_width;
  fb->height = tag_fb->common.framebuffer_height;
  fb->bpp = tag_fb->common.framebuffer_bpp;
  fb->x = 0;
  fb->y = 0;
  fb->red = 0xFF;
  fb->green = 0xFF;
  fb->blue = 0xFF;
  fb->dirty = 1;

  /* if rgb frame buffer, use default font */
  switch (fb->type) {
    case MULTIBOOT_FRAMEBUFFER_TYPE_RGB:
      fb->font = get_default_font();
      if (!fb->font)
        return -ENOSPC;
      fb->width_glyph = fb->width / fb->font->width;
      fb->height_glyph = fb->height / fb->font->height;
      fb->update = fb_update_rgb;
      break;
    case MULTIBOOT_FRAMEBUFFER_TYPE_EGA_TEXT:
      fb->font = NULL;
      fb->width_glyph = fb->width;
      fb->height_glyph = fb->height;
      fb->update = fb_update_text;
      break;
    default:
      return -EINVAL;
  }

  /* allocate buffer */
  fb->buf = (char *) kmalloc(fb->width_glyph * fb->height_glyph);
  if (!fb->buf)
    return -ENOMEM;

  /* identity map frame buffer */
  fb_nb_pages = div_ceil(fb->height * fb->pitch, PAGE_SIZE);
  for (i = 0; i < fb_nb_pages; i++)
    map_page_phys(fb->addr + i * PAGE_SIZE, fb->addr + i * PAGE_SIZE, kernel_pgd, 0, 1);

  return 0;
}

/*
 * Put a pixel on the screen.
 */
static inline void fb_put_pixel(struct framebuffer_t *fb, uint32_t x, uint32_t y, uint8_t red, uint8_t green, uint8_t blue)
{
  uint8_t *pixel = (uint8_t *) (fb->addr + x * 3 + y * fb->pitch);
  *pixel++ = red;
  *pixel++ = green;
  *pixel++ = blue;
}

/*
 * Print a blanck character on the frame buffer.
 */
static inline void fb_put_blank(struct framebuffer_t *fb, uint32_t pos_x, uint32_t pos_y)
{
  uint32_t y;

  for (y = 0; y < fb->font->height; y++)
    memset((void *) (fb->addr + pos_x * 3 + (pos_y + y) * fb->pitch), 0, fb->font->width * 3);
}

/*
 * Print a cursor on the frame buffer.
 */
static inline void fb_put_cursor(struct framebuffer_t *fb, uint32_t pos_x, uint32_t pos_y)
{
  uint32_t y;

  for (y = 0; y < fb->font->height; y++)
    memset((void *) (fb->addr + pos_x * 3 + (pos_y + y) * fb->pitch), 0xFF, fb->font->width * 3);
}

/*
 * Print a glyph on the frame buffer.
 */
static void fb_put_glyph(struct framebuffer_t *fb, int glyph, uint32_t pos_x, uint32_t pos_y)
{
  uint32_t x, y;
  uint8_t *font;
  uint8_t bit;

  /* invalid glyph */
  if (glyph < 0 || glyph >= fb->font->char_count) {
    fb_put_blank(fb, pos_x, pos_y);
    return;
  }

  /* get font */
  font = fb->font->data + glyph * fb->font->char_size;
  bit = 1 << 7;

  /* print glyph */
  for (y = 0; y < fb->font->height; y++) {
    for (x = 0; x < fb->font->width; x++) {
      if ((*font) & bit)
        fb_put_pixel(fb, pos_x + x, pos_y + y, fb->red, fb->green, fb->blue);
      else
        fb_put_pixel(fb, pos_x + x, pos_y + y, 0, 0, 0);

      /* go to next glyph */
      bit >>= 1;
      if (!bit) {
        bit = 1 << 7;
        font++;
      }
    }
  }
}

/*
 * Print a character on the frame buffer.
 */
static void fb_putc(struct framebuffer_t *fb, uint8_t c)
{
  uint32_t i;

  /* handle character */
  if (c >= ' ' && c <= '~') {
    fb->buf[fb->y * fb->width_glyph + fb->x] = c;
    fb->x++;
  } else if (c == '\b' && fb->x != 0) {
    fb->x--;
  } else if (c == '\t') {
    fb->x = (fb->x + 4) & ~0x03;
  } else if (c == '\n') {
    fb->y++;
    fb->x = 0;
  } else if (c == '\r') {
    fb->x = 0;
  }

  /* go to next line */
  if (fb->x >= fb->width_glyph) {
    fb->x = 0;
    fb->y++;
  }

  /* scroll */
  if (fb->y >= fb->height_glyph) {
    /* move each line up */
    for (i = 0; i < fb->width_glyph * (fb->height_glyph - 1); i++)
      fb->buf[i] = fb->buf[i + fb->width_glyph];

    /* clear last line */
    memset(&fb->buf[i], 0, fb->width_glyph);

    fb->y = fb->height_glyph - 1;
  }

  /* mark frame buffer dirty */
  fb->dirty = 1;
}

/*
 * Write a string to the frame buffer.
 */
size_t fb_write(struct framebuffer_t *fb, const char *buf, size_t n)
{
  size_t i;

  for (i = 0; i < n; i++)
    fb_putc(fb, buf[i]);

  return n;
}

/*
 * Update a text frame buffer.
 */
static void fb_update_text(struct framebuffer_t *fb)
{
  uint16_t *fb_buf = (uint16_t *) fb->addr;
  uint16_t pos;
  size_t i;

  /* copy the buffer */
  for (i = 0; i < fb->width_glyph * fb->height_glyph; i++)
    fb_buf[i] = TEXT_ENTRY(TEXT_BLACK, TEXT_LIGHT_GREY, fb->buf[i]);

  /* update hardware cursor */
  pos = fb->y * fb->width + fb->x;
  outb(0x03D4, 14);
  outb(0x03D5, pos >> 8);
  outb(0x03D4, 15);
  outb(0x03D5, pos);

  /* marke frame buffer clean */
  fb->dirty = 0;
}

/*
 * Update a RGB frame buffer.
 */
static void fb_update_rgb(struct framebuffer_t *fb)
{
  uint32_t x, y;
  char c;

  /* print each glyph */
  for (y = 0; y < fb->height_glyph; y++) {
    for (x = 0; x < fb->width_glyph; x++) {
      c = fb->buf[y * fb->width_glyph + x];
      fb_put_glyph(fb, c ? get_glyph(fb->font, c) : -1, x * fb->font->width, y * fb->font->height);
    }
  }

  /* add cursor */
  fb_put_cursor(fb, fb->x * fb->font->width, fb->y * fb->font->height);

  /* mark frame buffer clean */
  fb->dirty = 0;
}
