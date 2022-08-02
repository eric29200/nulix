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
	uint32_t		cursor_x;
	uint32_t		cursor_y;
	uint16_t *		buf;
	void			(*update_region)(struct framebuffer_t *, uint32_t, uint32_t);
	void			(*scroll)(struct framebuffer_t *);
	void			(*update_cursor)(struct framebuffer_t *);
	void			(*show_cursor)(struct framebuffer_t *, int);
};

int init_framebuffer(struct framebuffer_t *fb, struct multiboot_tag_framebuffer *tag_fb, uint16_t erase_char);
void fb_set_xy(struct framebuffer_t *fb, uint32_t x, uint32_t y);

/* text fb prototypes */
void fb_text_update_region(struct framebuffer_t *fb, uint32_t start, uint32_t len);
void fb_text_scroll(struct framebuffer_t *fb);
void fb_text_update_cursor(struct framebuffer_t *fb);
void fb_text_show_cursor(struct framebuffer_t *fb, int on_off);

/* rgb fb prototypes */
void fb_rgb_update_region(struct framebuffer_t *fb, uint32_t start, uint32_t len);
void fb_rgb_scroll(struct framebuffer_t *fb);
void fb_rgb_update_cursor(struct framebuffer_t *fb);
void fb_rgb_show_cursor(struct framebuffer_t *fb, int on_off);

#endif

