#include <drivers/fb.h>
#include <mm/mm.h>
#include <string.h>
#include <stderr.h>

/*
 * Init the framebuffer.
 */
int init_framebuffer(struct framebuffer_t *fb, struct multiboot_tag_framebuffer *tag_fb)
{
	uint32_t fb_nb_pages, i;
	uint32_t width, height;

	/* set frame buffer */
	fb->addr = tag_fb->common.framebuffer_addr;
	fb->type = tag_fb->common.framebuffer_type;
	fb->pitch = tag_fb->common.framebuffer_pitch;
	width = tag_fb->common.framebuffer_width;
	height = tag_fb->common.framebuffer_height;
	fb->bpp = tag_fb->common.framebuffer_bpp;
	fb->x = 0;
	fb->y = 0;
	fb->red = 0xFF;
	fb->green = 0xFF;
	fb->blue = 0xFF;
	fb->dirty = 1;

	/* init frame buffer */
	switch (fb->type) {
		case MULTIBOOT_FRAMEBUFFER_TYPE_EGA_TEXT:
			fb->font = NULL;
			fb->width = width;
			fb->height = height;
			fb->update = fb_text_update;
			break;
		case MULTIBOOT_FRAMEBUFFER_TYPE_RGB:
			fb->font = get_default_font();
			if (!fb->font)
				return -ENOSPC;
			fb->width = width / fb->font->width;
			fb->height = height / fb->font->height;
			fb->update = fb_rgb_update;
			break;
		default:
			return -EINVAL;
	}

	/* allocate buffer */
	fb->buf = (uint16_t *) kmalloc(sizeof(uint16_t) * fb->width * fb->height * fb->bpp / 8);
	if (!fb->buf)
		return -ENOMEM;

	/* identity map frame buffer */
	fb_nb_pages = div_ceil(height * fb->pitch, PAGE_SIZE);
	for (i = 0; i < fb_nb_pages; i++)
		map_page_phys(fb->addr + i * PAGE_SIZE, fb->addr + i * PAGE_SIZE, kernel_pgd, 0, 1);

	/* clear frame buffer */
	memset(fb->buf, 0, sizeof(uint16_t) * fb->width * fb->height * fb->bpp / 8);

	return 0;
}

/*
 * Set framebuffer position.
 */
void fb_set_xy(struct framebuffer_t *fb, uint32_t x, uint32_t y)
{
	if (!fb)
		return;

	fb->x = x;
	fb->y = y;
}

/*
 * Print a character on the frame buffer.
 */
void fb_putc(struct framebuffer_t *fb, uint8_t c, uint8_t color)
{
	uint32_t i;

	/* handle character */
	if (c >= ' ' && c <= '~') {
		fb->buf[fb->y * fb->width + fb->x] = (color << 8) | c;
		fb->x++;
	} else if (c == '\t') {
		fb->x = (fb->x + fb->bpp / 8) & ~0x03;
	} else if (c == '\n') {
		fb->y++;
		fb->x = 0;
	} else if (c == '\r') {
		fb->x = 0;
	} else if (c == '\b') {
		fb->x--;
	}

	/* go to next line */
	if (fb->x >= fb->width) {
		fb->x = 0;
		fb->buf[fb->y * fb->width + fb->x] = 0;
	}

	/* scroll */
	if (fb->y >= fb->height) {
		/* move each line up */
		for (i = 0; i < fb->width * (fb->height - 1); i++)
			fb->buf[i] = fb->buf[i + fb->width];

		/* clear last line */
		memset(&fb->buf[i], 0, fb->width * 2);

		fb->y = fb->height - 1;
	}

	/* mark frame buffer dirty */
	fb->dirty = 1;
}
