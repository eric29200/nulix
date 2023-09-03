#include <drivers/video/fb.h>
#include <string.h>
#include <stderr.h>

#define DEFAULT_FONT_WIDTH		8
#define DEFAULT_FONT_HEIGHT		16

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
 * Put a pixel on the screen.
 */
static inline void fb_put_pixel(struct framebuffer *fb, uint32_t x, uint32_t y, uint32_t color)
{
	uint8_t *pixel = (uint8_t *) (fb->addr + x * fb->bpp / 8 + y * fb->pitch);

	*pixel++ = color & 0xFF;
	*pixel++ = (color >> 8) & 0xFF;
	*pixel++ = (color >> 16) & 0xFF;
}

/*
 * Print a glyph on the frame buffer.
 */
static void fb_put_glyph(struct framebuffer *fb, int glyph, uint32_t pos_x, uint32_t pos_y, uint32_t color_bg, uint32_t color_fg)
{
	uint32_t x, y;
	uint8_t *font;
	uint8_t bit;

	/* get font */
	font = &fb->font->data[glyph * fb->font->height];
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
 * Get RGB foreground color from ansi color.
 */
static uint32_t fb_rgb_fg_color(uint8_t color)
{
	int fg, bright;

	fg = color & 7;
	bright = (color & 0xF) & 8;

	return rgb_color_table[bright + fg];
}

/*
 * Get RGB background color from ansi color.
 */
static uint32_t fb_rgb_bg_color(uint8_t color)
{
	int bg = (color >> 4) & 7;

	return rgb_color_table[bg];
}

/*
 * Update cursor.
 */
static void fb_rgb_update_cursor(struct framebuffer *fb)
{
	uint32_t color_bg, color_fg;
	uint8_t c, color;

	/* draw cursor */
	c = fb->buf[fb->y * fb->width + fb->x];
	color = fb->buf[fb->y * fb->width + fb->x] >> 8;
	color_bg = fb_rgb_bg_color(color);
	color_fg = fb_rgb_fg_color(color);
	fb_put_glyph(fb, c, fb->x * fb->font->width, fb->y * fb->font->height, color_fg, color_bg);

	/* set new cusor */
	fb->cursor_x = fb->x;
	fb->cursor_y = fb->y;
}

/*
 * Update a frame buffer region.
 */
static void fb_rgb_update_region(struct framebuffer *fb, uint32_t start, uint32_t len)
{
	uint32_t color_bg, color_fg, i, x, y;
	uint8_t c, color;

	for (i = 0; i < len; i++) {
		/* compute coordinates */
		x = (start + i) % fb->width;
		y = (start + i) / fb->width;

		/* get character and color */
		c = fb->buf[y * fb->width + x];
		color = fb->buf[y * fb->width + x] >> 8;
		color_bg = fb_rgb_bg_color(color);
		color_fg = fb_rgb_fg_color(color);

		/* put glyph */
		fb_put_glyph(fb, c, x * fb->font->width, y * fb->font->height, color_bg, color_fg);
	}
}

/*
 * Show/Hide cursor.
 */
static void fb_rgb_show_cursor(struct framebuffer *fb, int on_off)
{
	if (on_off)
		fb_rgb_update_cursor(fb);
	else
		fb_rgb_update_region(fb, fb->cursor_y * fb->width + fb->cursor_x, 1);
}

/*
 * Scroll up from bottom to top.
 */
static void fb_rgb_scroll_up(struct framebuffer *fb, uint32_t top, uint32_t bottom, size_t nr)
{
	uint32_t *src, *dest;

	/* scroll up */
	dest = (uint32_t *) (fb->addr + fb->font->height * fb->pitch * top);
	src = (uint32_t *) (fb->addr + fb->font->height * fb->pitch * (top + nr));
	memmovedw(dest, src, fb->font->height * fb->pitch * (bottom - top - nr) / sizeof(uint32_t));

	/* update last lines */
	fb_rgb_update_region(fb, fb->width * (bottom - top - nr), fb->width * nr);
}

/*
 * Scroll down from top to bottom.
 */
static void fb_rgb_scroll_down(struct framebuffer *fb, uint32_t top, uint32_t bottom, size_t nr)
{
	uint32_t *src, *dest;

	/* scroll down */
	dest = (uint32_t *) (fb->addr + fb->font->height * fb->pitch * (top + nr));
	src = (uint32_t *) (fb->addr + fb->font->height * fb->pitch * top);
	memmovedw(dest, src, fb->font->height * fb->pitch * (bottom - top - nr) / sizeof(uint32_t));

	/* update first lines */
	fb_rgb_update_region(fb, top * fb->width, fb->width * nr);
}

/*
 * Init framebuffer.
 */
static int fb_rgb_init(struct framebuffer *fb)
{
	/* get font */
	fb->font = font_find(DEFAULT_FONT_WIDTH, DEFAULT_FONT_HEIGHT);
	if (!fb->font)
		return -EINVAL;

	fb->width = fb->real_width / fb->font->width;
	fb->height = fb->real_height / fb->font->height;

	return 0;
}

/*
 * Get frame buffer fix informations.
 */
static int fb_rgb_get_fix(struct framebuffer *fb, struct fb_fix_screeninfo *fix)
{
	/* reset informations */
	memset(fix, 0, sizeof(struct fb_fix_screeninfo));

	/* set informations */
	strcpy(fix->id, "VESA VGA");
	fix->smem_start = fb->addr;
	fix->smem_len = fb->real_height * fb->pitch;
	fix->type = FB_TYPE_PACKED_PIXELS;
	fix->visual = FB_VISUAL_TRUECOLOR;
	fix->xpanstep = 0;
	fix->ypanstep = 0;
	fix->ywrapstep = 0;
	fix->line_length = fb->pitch;

	return 0;
}

/*
 * Get framebuffer variable informations.
 */
static int fb_rgb_get_var(struct framebuffer *fb, struct fb_var_screeninfo *var)
{
	/* reset informations */
	memset(var, 0, sizeof(struct fb_var_screeninfo));

	/* set informations */
	var->xres = fb->real_width;
	var->yres = fb->real_height;
	var->xres_virtual = fb->real_width;
	var->yres_virtual = fb->real_height;
	var->bits_per_pixel = fb->bpp;
	var->red.offset = fb->tag_fb->framebuffer_red_field_position;
	var->red.length = fb->tag_fb->framebuffer_red_mask_size;
	var->green.offset = fb->tag_fb->framebuffer_green_field_position;
	var->green.length = fb->tag_fb->framebuffer_green_mask_size;
	var->blue.offset = fb->tag_fb->framebuffer_blue_field_position;
	var->blue.length = fb->tag_fb->framebuffer_blue_mask_size;

	return 0;
}

/*
 * Put framebuffer variable informations.
 */
static int fb_rgb_put_var(struct framebuffer *fb, struct fb_var_screeninfo *var)
{
	/* set informations */
	fb->real_width = var->xres;
	fb->real_height = var->yres;
	fb->real_width = var->xres_virtual;
	fb->real_height = var->yres_virtual;
	fb->bpp = var->bits_per_pixel;
	fb->tag_fb->framebuffer_red_field_position = var->red.offset;
	fb->tag_fb->framebuffer_red_mask_size = var->red.length;
	fb->tag_fb->framebuffer_green_field_position = var->green.offset;
	fb->tag_fb->framebuffer_green_mask_size = var->green.length;
	fb->tag_fb->framebuffer_blue_field_position = var->blue.offset;
	fb->tag_fb->framebuffer_blue_mask_size = var->blue.length;

	return 0;
}

/*
 * RGB frame buffer operations.
 */
struct framebuffer_ops fb_rgb_ops = {
	.init			= fb_rgb_init,
	.update_region		= fb_rgb_update_region,
	.scroll_up		= fb_rgb_scroll_up,
	.scroll_down		= fb_rgb_scroll_down,
	.update_cursor		= fb_rgb_update_cursor,
	.show_cursor		= fb_rgb_show_cursor,
	.get_fix		= fb_rgb_get_fix,
	.get_var		= fb_rgb_get_var,
	.put_var		= fb_rgb_put_var,
};
