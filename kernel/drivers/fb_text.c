#include <drivers/fb.h>
#include <x86/io.h>
#include <string.h>

/*
 * Update cursor.
 */
void fb_text_update_cursor(struct framebuffer_t *fb)
{
	uint16_t pos = fb->y * fb->width + fb->x;

	/* update hardware cursor */
	outb(0x03D4, 14);
	outb(0x03D5, (pos >> 8) & 0xFF);
	outb(0x03D4, 15);
	outb(0x03D5, pos & 0xFF);
}

/*
 * Show/Hide cursor.
 */
void fb_text_show_cursor(struct framebuffer_t *fb, int on_off)
{
	uint8_t status;

	UNUSED(fb);

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
 * Update a text frame buffer.
 */
void fb_text_update(struct framebuffer_t *fb)
{
	uint16_t *fb_buf = (uint16_t *) fb->addr;
	size_t i;

	/* copy the buffer */
	for (i = 0; i < fb->width * fb->height; i++)
		fb_buf[i] = TEXT_ENTRY(fb->buf[i], fb->buf[i] >> 8);
}

/*
 * Update a frame buffer region.
 */
void fb_text_update_region(struct framebuffer_t *fb, uint32_t start, uint32_t len)
{
	uint16_t *fb_buf = (uint16_t *) fb->addr;
	uint32_t i;

	for (i = 0; i < len; i++)
		fb_buf[start + i] = TEXT_ENTRY(fb->buf[start + i], fb->buf[start + i] >> 8);
}

/*
 * Scroll framebuffer.
 */
void fb_text_scroll_up(struct framebuffer_t *fb, uint32_t top, uint32_t bottom, size_t nr)
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
void fb_text_scroll_down(struct framebuffer_t *fb, uint32_t top, uint32_t bottom, size_t nr)
{
	uint16_t *src, *dest;

	/* scroll down */
	dest = (uint16_t *) (fb->addr + fb->width * (top + nr) * sizeof(uint16_t));
	src = (uint16_t *) (fb->addr + fb->width * top * sizeof(uint16_t));
	memmovew(dest, src, fb->width * (bottom - top - nr));

	/* update first lines */
	fb_text_update_region(fb, top * fb->width, fb->width * nr);
}
