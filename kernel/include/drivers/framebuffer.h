#ifndef _FRAMEBUFFER_H_
#define _FRAMEBUFFER_H_

#include <grub/multiboot2.h>
#include <stddef.h>
#include <font.h>

/*
 * Frame buffer structure.
 */
struct framebuffer_t {
  uint32_t        addr;
  uint32_t        pitch;
  uint32_t        width;
  uint32_t        height;
  uint8_t         bpp;
  struct font_t   font;
  uint32_t        x;
  uint32_t        y;
  uint8_t         red;
  uint8_t         green;
  uint8_t         blue;
};

int init_framebuffer(struct framebuffer_t *fb, struct multiboot_tag_framebuffer *tag_fb);
void fb_write(struct framebuffer_t *fb, const char *buf, size_t n);

#endif
