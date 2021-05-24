#ifndef _FRAMEBUFFER_H_
#define _FRAMEBUFFER_H_

#include <grub/multiboot2.h>
#include <stddef.h>

/*
 * Frame buffer structure.
 */
struct framebuffer_t {
  uint32_t  addr;
  uint32_t  pitch;
  uint32_t  width;
  uint32_t  height;
  uint8_t   bpp;
};

void init_framebuffer(struct multiboot_tag_framebuffer *tag_fb);

#endif
