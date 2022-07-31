#ifndef _FB_H_
#define _FB_H_

#include <grub/multiboot2.h>
#include <stddef.h>
#include <lib/font.h>

#define TEXT_BLACK		0
#define TEXT_LIGHT_GREY		7
#define TEXT_COLOR(bg, fg)	(((bg) << 4) | (fg))
#define TEXT_DEF_COLOR		TEXT_COLOR(TEXT_BLACK, TEXT_LIGHT_GREY)
#define TEXT_ENTRY(c, color)	(((color) << 8) | (c))


/*
 * Frame buffer structure.
 */
struct framebuffer_t {
	uint32_t		addr;
	uint16_t		type;
	uint32_t		pitch;
	uint32_t		width;
	uint32_t		height;
	uint8_t			bpp;
	struct font_t *		font;
	uint32_t		x;
	uint32_t		y;
	uint16_t *		buf;
	char			dirty;
	void			(*update)(struct framebuffer_t *);
};

int init_framebuffer(struct framebuffer_t *fb, struct multiboot_tag_framebuffer *tag_fb);
void fb_set_xy(struct framebuffer_t *fb, uint32_t x, uint32_t y);

void fb_text_update(struct framebuffer_t *fb);
void fb_rgb_update(struct framebuffer_t *fb);

#endif

