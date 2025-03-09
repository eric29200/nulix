#include <drivers/video/fb.h>
#include <x86/io.h>
#include <string.h>

/*
 * Clear cursor.
 */
static void fb_text_clear_cursor(struct framebuffer *fb)
{
	UNUSED(fb);
}

/*
 * Update cursor.
 */
static void fb_text_update_cursor(struct framebuffer *fb)
{
	uint16_t pos = fb->y * fb->width + fb->x;

	/* update position */
	fb->cursor_x = fb->x;
	fb->cursor_y = fb->y;

	/* update hardware cursor */
	if (fb->active && fb->cursor_on) {
		outb(0x03D4, 14);
		outb(0x03D5, (pos >> 8) & 0xFF);
		outb(0x03D4, 15);
		outb(0x03D5, pos & 0xFF);
	}
}

/*
 * Show/Hide cursor.
 */
static void fb_text_show_cursor(struct framebuffer *fb, int on_off)
{
	uint8_t status;

	/* switch on/off cursor */
	fb->cursor_on = on_off;
	switch (on_off) {
		case 0:
			outb(0x03D4, 0xA);
			status = inb(0x03D5);
			outb(0x03D5, status | 0x20);
			break;
		default:
			outb(0x03D4, 0xA);
			status = inb(0x03D5);
			outb(0x03D5, status & 0x1F);
			break;
	}
}

/*
 * Update a frame buffer region.
 */
static void fb_text_update_region(struct framebuffer *fb, uint32_t start, uint32_t len)
{
	uint16_t *fb_buf = (uint16_t *) fb->addr;
	uint32_t i;

	for (i = 0; i < len; i++)
		fb_buf[start + i] = TEXT_ENTRY(fb->buf[start + i], fb->buf[start + i] >> 8);
}

/*
 * Scroll framebuffer.
 */
static void fb_text_scroll_up(struct framebuffer *fb, uint32_t top, uint32_t bottom, size_t nr)
{
	uint16_t *src, *dest;

	/* scroll up */
	dest = (uint16_t *) (fb->addr + fb->width * top * sizeof(uint16_t));
	src = (uint16_t *) (fb->addr + fb->width * (top + nr) * sizeof(uint16_t));
	memmovew(dest, src, fb->width * (bottom - top - nr));

	/* update last line */
	fb_text_update_region(fb, fb->width * (bottom - top - nr), fb->width * nr);
}

/*
 * Scroll down from top to bottom.
 */
static void fb_text_scroll_down(struct framebuffer *fb, uint32_t top, uint32_t bottom, size_t nr)
{
	uint16_t *src, *dest;

	/* scroll down */
	dest = (uint16_t *) (fb->addr + fb->width * (top + nr) * sizeof(uint16_t));
	src = (uint16_t *) (fb->addr + fb->width * top * sizeof(uint16_t));
	memmovew(dest, src, fb->width * (bottom - top - nr));

	/* update first lines */
	fb_text_update_region(fb, top * fb->width, fb->width * nr);
}

/*
 * Init framebuffer.
 */
static int fb_text_init(struct framebuffer *fb)
{
	fb->width = fb->real_width;
	fb->height = fb->real_height;
	fb->font = NULL;

	return 0;
}

/*
 * Text frame buffer operations.
 */
struct framebuffer_ops fb_text_ops = {
	.init			= fb_text_init,
	.update_region		= fb_text_update_region,
	.scroll_up		= fb_text_scroll_up,
	.scroll_down		= fb_text_scroll_down,
	.clear_cursor		= fb_text_clear_cursor,
	.update_cursor		= fb_text_update_cursor,
	.show_cursor		= fb_text_show_cursor,
};
