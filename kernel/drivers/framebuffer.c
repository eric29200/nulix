#include <drivers/framebuffer.h>
#include <drivers/tty.h>
#include <fs/fs.h>
#include <mm/mm.h>
#include <x86/io.h>
#include <lib/font.h>
#include <string.h>
#include <stderr.h>

/* frame buffer update functions */
static void fb_update_text(struct framebuffer_t *fb);
static void fb_update_rgb(struct framebuffer_t *fb);

/* RGB color table */
static uint32_t rgb_color_table[] = {
	0x000000,	/* black */
	0x0000AA,	/* blue */
	0x00AA00,	/* green */
	0x00AAAA,	/* cyan */
	0xAA0000,	/* red */
	0xAA00AA,	/* magenta */
	0xAA5000,	/* brown */
	0xAAAAAA,	/* light gray */
	0x555555,	/* dark gray */
	0x5555FF,	/* light blue */
	0x55FF55,	/* light green */
	0x55FFFF,	/* light cyan */
	0xFF5555,	/* light red */
	0xFF55FF,	/* light magenta */
	0xFFFF55,	/* yellow */
	0xFFFFFF	/* white */
};

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
	if (fb->type == MULTIBOOT_FRAMEBUFFER_TYPE_EGA_TEXT) {
		fb->font = NULL;
		fb->width = width;
		fb->height = height;
		fb->update = fb_update_text;
	} else if (fb->type == MULTIBOOT_FRAMEBUFFER_TYPE_RGB) {
		fb->font = get_default_font();
		if (!fb->font)
			return -ENOSPC;
		fb->width = width / fb->font->width;
		fb->height = height / fb->font->height;
		fb->update = fb_update_rgb;
	} else {
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
 * Put a pixel on the screen.
 */
static inline void fb_put_pixel(struct framebuffer_t *fb, uint32_t x, uint32_t y, uint32_t color)
{
	uint8_t *pixel = (uint8_t *) (fb->addr + x * fb->bpp / 8 + y * fb->pitch);
	*pixel++ = color & 0xFF;
	*pixel++ = (color >> 8) & 0xFF;
	*pixel++ = (color >> 16) & 0xFF;
}

/*
 * Print a blanck character on the frame buffer.
 */
static inline void fb_put_blank(struct framebuffer_t *fb, uint32_t pos_x, uint32_t pos_y)
{
	uint32_t y;

	for (y = 0; y < fb->font->height; y++)
		memset((void *) (fb->addr + pos_x * fb->bpp / 8 + (pos_y + y) * fb->pitch), 0, fb->font->width * fb->bpp / 8);
}

/*
 * Print a cursor on the frame buffer.
 */
static inline void fb_put_cursor(struct framebuffer_t *fb, uint32_t pos_x, uint32_t pos_y)
{
	uint32_t y;

	for (y = 0; y < fb->font->height; y++)
		memset((void *) (fb->addr + pos_x * fb->bpp / 8 + (pos_y + y) * fb->pitch), 0xFF, fb->font->width * fb->bpp / 8);
}

/*
 * Print a glyph on the frame buffer.
 */
static void fb_put_glyph(struct framebuffer_t *fb, int glyph, uint32_t pos_x, uint32_t pos_y, uint32_t color_bg, uint32_t color_fg)
{
	uint32_t x, y;
	uint8_t *font;
	uint8_t bit;

	/* invalid glyph */
	if (glyph < 0 || glyph >= fb->font->char_count) {
		fb_put_blank(fb, pos_x, pos_y);
		return;
	}

	/* get font */
	font = fb->font->data + glyph * fb->font->char_size;
	bit = 1 << 7;

	/* print glyph */
	for (y = 0; y < fb->font->height; y++) {
		for (x = 0; x < fb->font->width; x++) {
			if (*font & bit)
				fb_put_pixel(fb, pos_x + x, pos_y + y, color_fg);
			else
				fb_put_pixel(fb, pos_x + x, pos_y + y, color_bg);

			/* go to next glyph */
			bit >>= 1;
			if (!bit) {
				bit = 1 << 7;
				font++;
			}
		}
	}
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
 * Update a text frame buffer.
 */
static void fb_update_text(struct framebuffer_t *fb)
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

/*
 * Get RGB foreground color from ansi color.
 */
static uint32_t fb_get_rgb_fg_color(uint8_t color)
{
	int fg, bright;

	fg = color & 7;
	bright = (color & 0xF) & 8;

	return rgb_color_table[bright + fg];
}

/*
 * Get RGB background color from ansi color.
 */
static uint32_t fb_get_rgb_bg_color(uint8_t color)
{
	int bg = (color >> 4) & 7;

	return rgb_color_table[bg];
}

/*
 * Update a RGB frame buffer.
 */
static void fb_update_rgb(struct framebuffer_t *fb)
{
	uint32_t color_bg, color_fg;
	uint8_t c, color;
	uint32_t x, y;

	/* print each glyph */
	for (y = 0; y < fb->height; y++) {
		for (x = 0; x < fb->width; x++) {
			/* get character and color */
			c = fb->buf[y * fb->width + x];
			color = fb->buf[y * fb->width + x] >> 8;
			color_bg = fb_get_rgb_bg_color(color);
			color_fg = fb_get_rgb_fg_color(color);

			/* put glyph */
			fb_put_glyph(fb, c ? get_glyph(fb->font, c) : -1, x * fb->font->width, y * fb->font->height, color_bg, color_fg);
		}
	}

	/* add cursor */
	fb_put_cursor(fb, fb->x * fb->font->width, fb->y * fb->font->height);

	/* mark frame buffer clean */
	fb->dirty = 0;
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
