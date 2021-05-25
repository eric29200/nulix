#include <drivers/framebuffer.h>
#include <string.h>
#include <mm/mm.h>
#include <font.h>

/* framebuffer */
static struct framebuffer_t fb;

/* default font */
extern char _binary_fonts_ter_powerline_v16n_psf_start;
extern char _binary_fonts_ter_powerline_v16n_psf_end;

/*
 * Init the framebuffer.
 */
int init_framebuffer(struct multiboot_tag_framebuffer *tag_fb)
{
  uint32_t fb_nb_pages, i;
  int ret;

  fb.addr = tag_fb->common.framebuffer_addr;
  fb.pitch = tag_fb->common.framebuffer_pitch;
  fb.width = tag_fb->common.framebuffer_width;
  fb.height = tag_fb->common.framebuffer_height;
  fb.bpp = tag_fb->common.framebuffer_bpp;
  fb.x = 0;
  fb.y = 0;
  fb.red = 0xFF;
  fb.green = 0xFF;
  fb.blue = 0xFF;

  /* load font */
  ret = load_font(&fb.font, &_binary_fonts_ter_powerline_v16n_psf_start, &_binary_fonts_ter_powerline_v16n_psf_end);
  if (ret != 0)
    return ret;

  /* identity map frame buffer */
  fb_nb_pages = div_ceil(fb.height * fb.pitch, PAGE_SIZE);
  for (i = 0; i < fb_nb_pages; i++)
    map_page_phys(fb.addr + i * PAGE_SIZE, fb.addr + i * PAGE_SIZE, kernel_pgd, 0, 1);

  return 0;
}

/*
 * Put a pixel on the screen.
 */
static void fb_put_pixel(uint32_t x, uint32_t y, uint8_t red, uint8_t green, uint8_t blue)
{
  uint8_t *pixel = (uint8_t *) (fb.addr + x * 3 + y * fb.pitch);
  *pixel++ = red;
  *pixel++ = green;
  *pixel++ = blue;
}

/*
 * Print a blanck character on the frame buffer.
 */
static void fb_putblank()
{
  uint32_t x, y;

  /* print glyph */
  for (y = 0; y < fb.font.height; y++)
    for (x = 0; x < fb.font.width; x++)
      if (x == 1 || x == fb.font.width - 2 || y == 1 || y == fb.font.height - 2)
        fb_put_pixel(fb.x + x, fb.y + y, fb.red, fb.green, fb.blue);
      else
        fb_put_pixel(fb.x + x, fb.y + y, 0, 0, 0);

  /* update cursor */
  fb.x += fb.font.width;
}

/*
 * Print a glyph on the frame buffer.
 */
static void fb_putglyph(uint16_t glyph)
{
  uint32_t x, y;
  uint8_t bit;
  uint8_t *font;

  /* invalid glyph */
  if (glyph >= fb.font.char_count) {
    fb_putblank();
    return;
  }

  /* get font */
  font = fb.font.data + glyph * fb.font.char_size;
  bit = 1 << 7;

  /* print glyph */
  for (y = 0; y < fb.font.height; y++) {
    for (x = 0; x < fb.font.width; x++) {
      if (x < fb.font.width) {
        if ((*font) & bit)
          fb_put_pixel(fb.x + x, fb.y + y, fb.red, fb.green, fb.blue);
        else
          fb_put_pixel(fb.x + x, fb.y + y, 0, 0, 0);
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
  fb.x += fb.font.width;
}

/*
 * Clear the frame buffer.
 */
static void fb_clear()
{
  uint32_t h, y;
  uint8_t *p;

  for (y = 0; y < fb.height - fb.font.height; y++) {
    for (h = 0; h < fb.font.height; h++) {
      p = (uint8_t *) (fb.addr + (y + h) * fb.pitch);
      memset(p, 0, fb.width);
    }
  }
}

/*
 * Print a character on the frame buffer.
 */
void fb_putc(uint8_t c)
{
  int glyph;

  /* handle new character */
  switch (c) {
    case '\r':
      fb.x = 0;
      break;
    case '\n':
      fb.x = 0;
      fb.y += fb.font.height;
      break;
    default:
      glyph = get_glyph(&fb.font, c);

      if (glyph < 0)
        fb_putblank();
      else
        fb_putglyph(glyph);

      break;
  }

  /* go to next line */
  if (fb.x + fb.font.width > fb.width) {
    fb.x = 0;
    fb.y += fb.font.height;
  }

  /* end of frame buffer : blank screen */
  if (fb.y + fb.font.height > fb.height) {
    fb_clear();
    fb.y = 0;
  }
}

/*
 * Write a string to the frame buffer.
 */
void fb_write(const char *buf, size_t n)
{
  size_t i;

  for (i = 0; i < n; i++)
    fb_putc(buf[i]);
}
