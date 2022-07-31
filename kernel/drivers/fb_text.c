#include <drivers/fb.h>
#include <x86/io.h>

/*
 * Update a text frame buffer.
 */
void fb_text_update(struct framebuffer_t *fb)
{
	uint16_t *fb_buf = (uint16_t *) fb->addr;
	uint16_t pos;
	size_t i;

	/* copy the buffer */
	for (i = 0; i < fb->width * fb->height; i++)
		fb_buf[i] = TEXT_ENTRY(fb->buf[i], fb->buf[i] >> 8);

	/* update hardware cursor */
	pos = fb->y * fb->width + fb->x;
	outb(0x03D4, 14);
	outb(0x03D5, pos >> 8);
	outb(0x03D4, 15);
	outb(0x03D5, pos);

	/* marke frame buffer clean */
	fb->dirty = 0;
}
