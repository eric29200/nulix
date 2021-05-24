#include <drivers/framebuffer.h>
#include <mm/mm.h>

/* framebuffer */
static struct framebuffer_t fb;

/*
 * Init the framebuffer.
 */
void init_framebuffer(struct multiboot_tag_framebuffer *tag_fb)
{
  uint32_t fb_nb_pages, i;

  fb.addr = tag_fb->common.framebuffer_addr;
  fb.pitch = tag_fb->common.framebuffer_pitch;
  fb.width = tag_fb->common.framebuffer_width;
  fb.height = tag_fb->common.framebuffer_height;
  fb.bpp = tag_fb->common.framebuffer_bpp;

  /* identity map frame buffer */
  fb_nb_pages = div_ceil(fb.height * fb.pitch, PAGE_SIZE);
  for (i = 0; i < fb_nb_pages; i++)
    map_page_phys(fb.addr + i * PAGE_SIZE, fb.addr + i * PAGE_SIZE, kernel_pgd, 0, 1);
}
