#include <drivers/framebuffer.h>

/* framebuffer */
static struct framebuffer_t fb;

/*
 * Init the framebuffer.
 */
void init_framebuffer(struct multiboot_tag_framebuffer *tag_fb)
{
  fb.addr = tag_fb->common.framebuffer_addr;
  fb.pitch = tag_fb->common.framebuffer_pitch;
  fb.width = tag_fb->common.framebuffer_width;
  fb.height = tag_fb->common.framebuffer_height;
  fb.bpp = tag_fb->common.framebuffer_bpp;
}
