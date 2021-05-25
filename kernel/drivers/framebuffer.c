#include <drivers/framebuffer.h>
#include <string.h>
#include <mm/mm.h>
#include <x86/io.h>
#include <font.h>
#include <stderr.h>

/* put char functions */
static void fb_glyph_putc(struct framebuffer_t *fb, uint8_t c);
static void fb_text_putc(struct framebuffer_t *fb, uint8_t c);

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
      fb->buf_size = fb->width * fb->height / (fb->font->width * fb->font->height);
      break;
    case MULTIBOOT_FRAMEBUFFER_TYPE_EGA_TEXT:
      fb->font = NULL;
      fb->buf_size = fb->width * fb->height;
      break;
    default:
      return -EINVAL;
  }

  /* allocate buffer */
  fb->buf = (char *) kmalloc(fb->buf_size);
  if (!fb->buf)
    return -ENOMEM;

  /* identity map frame buffer */
  fb_nb_pages = div_ceil(fb->height * fb->pitch, PAGE_SIZE);
  for (i = 0; i < fb_nb_pages; i++)
    map_page_phys(fb->addr + i * PAGE_SIZE, fb->addr + i * PAGE_SIZE, kernel_pgd, 0, 1);

  return 0;
}

/*
 * Clear the frame buffer.
 */
static inline void fb_clear(struct framebuffer_t *fb)
{
  memset((void *) fb->addr, 0, fb->height * fb->pitch);
}

/*
 * Put a pixel on the screen.
 */
static void fb_put_pixel(struct framebuffer_t *fb, uint32_t x, uint32_t y, uint8_t red, uint8_t green, uint8_t blue)
{
  uint8_t *pixel = (uint8_t *) (fb->addr + x * 3 + y * fb->pitch);
  *pixel++ = red;
  *pixel++ = green;
  *pixel++ = blue;
}

/*
 * Print a blanck character on the frame buffer.
 */
static void fb_putblank(struct framebuffer_t *fb)
{
  uint32_t x, y;

  /* print glyph */
  for (y = 0; y < fb->font->height; y++)
    for (x = 0; x < fb->font->width; x++)
      if (x == 1 || x == fb->font->width - 2 || y == 1 || y == fb->font->height - 2)
        fb_put_pixel(fb, fb->x + x, fb->y + y, fb->red, fb->green, fb->blue);
      else
        fb_put_pixel(fb, fb->x + x, fb->y + y, 0, 0, 0);

  /* update cursor */
  fb->x += fb->font->width;
}

/*
 * Print a glyph on the frame buffer.
 */
static void fb_putglyph(struct framebuffer_t *fb, uint16_t glyph)
{
  uint32_t x, y;
  uint8_t bit;
  uint8_t *font;

  /* invalid glyph */
  if (glyph >= fb->font->char_count) {
    fb_putblank(fb);
    return;
  }

  /* get font */
  font = fb->font->data + glyph * fb->font->char_size;
  bit = 1 << 7;

  /* print glyph */
  for (y = 0; y < fb->font->height; y++) {
    for (x = 0; x < fb->font->width; x++) {
      if (x < fb->font->width) {
        if ((*font) & bit)
          fb_put_pixel(fb, fb->x + x, fb->y + y, fb->red, fb->green, fb->blue);
        else
          fb_put_pixel(fb, fb->x + x, fb->y + y, 0, 0, 0);
      }

      /* go to next glyph */
      bit >>= 1;
      if (!bit) {
        bit = 1 << 7;
        font++;
      }
    }
  }

  /* update cursor */
  fb->x += fb->font->width;
}

/*
 * Print a unicode character on the frame buffer.
 */
static void fb_glyph_putc(struct framebuffer_t *fb, uint8_t c)
{
  int glyph;

  /* handle new character */
  switch (c) {
    case '\r':
      fb->x = 0;
      break;
    case '\n':
      fb->x = 0;
      fb->y += fb->font->height;
      break;
    default:
      glyph = get_glyph(fb->font, c);

      if (glyph < 0)
        fb_putblank(fb);
      else
        fb_putglyph(fb, glyph);

      break;
  }

  /* go to next line */
  if (fb->x + fb->font->width > fb->width) {
    fb->x = 0;
    fb->y += fb->font->height;
  }

  /* end of frame buffer : blank screen */
  if (fb->y + fb->font->height > fb->height) {
    fb_clear(fb);
    fb->y = 0;
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
    fb->buf[fb->y * fb->width + fb->x] = c;
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
  if (fb->x >= fb->width) {
    fb->x = 0;
    fb->y++;
  }

  /* scroll */
  if (fb->y >= fb->height) {
    /* move each line up */
    for (i = 0; i < fb->width * (fb->height - 1); i++)
      fb->buf[i] = fb->buf[i + fb->width];

    /* clear last line */
    memset(&fb->buf[i], 0, fb->width);

    fb->y = fb->height - 1;
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
 * Update a frame buffer.
 */
void fb_update(struct framebuffer_t *fb)
{
  uint16_t pos = fb->y * fb->width + fb->x;
  uint16_t *video_buf = (uint16_t *) fb->addr;
  size_t i;

  /* copy the buffer */
  for (i = 0; i < fb->width * fb->height; i++)
    video_buf[i] = TEXT_ENTRY(TEXT_BLACK, TEXT_LIGHT_GREY, fb->buf[i]);

  /* update hardware cursor */
  outb(0x03D4, 14);
  outb(0x03D5, pos >> 8);
  outb(0x03D4, 15);
  outb(0x03D5, pos);

  fb->dirty = 0;
}
