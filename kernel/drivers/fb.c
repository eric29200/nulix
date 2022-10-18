#include <drivers/fb.h>
#include <mm/mm.h>
#include <string.h>
#include <stderr.h>

/* direct frame buffer */
static struct framebuffer_t direct_fb;

/*
 * Init direct frame buffer.
 */
int init_framebuffer_direct(struct multiboot_tag_framebuffer *tag_fb)
{
  return init_framebuffer(&direct_fb, tag_fb, 0, 1);
}

/*
 * Init the framebuffer.
 */
int init_framebuffer(struct framebuffer_t *fb, struct multiboot_tag_framebuffer *tag_fb, uint16_t erase_char, int direct)
{
	uint32_t fb_nb_pages, i;
	int ret;

	/* set frame buffer */
	fb->addr = tag_fb->common.framebuffer_addr;
	fb->type = tag_fb->common.framebuffer_type;
	fb->pitch = tag_fb->common.framebuffer_pitch;
	fb->real_width = tag_fb->common.framebuffer_width;
	fb->real_height = tag_fb->common.framebuffer_height;
	fb->bpp = tag_fb->common.framebuffer_bpp;
	fb->x = fb->cursor_x = 0;
	fb->y = fb->cursor_y = 0;
	fb->active = 0;

	/* direct frame buffer */
	if (direct == 1)
		return 0;

	/* init frame buffer */
	switch (fb->type) {
		case MULTIBOOT_FRAMEBUFFER_TYPE_EGA_TEXT:
			fb->font = NULL;
			fb->width = fb->real_width;
			fb->height = fb->real_height;
			fb->update_region = fb_text_update_region;
			fb->scroll_up = fb_text_scroll_up;
			fb->scroll_down = fb_text_scroll_down;
			fb->update_cursor = fb_text_update_cursor;
			fb->show_cursor = fb_text_show_cursor;
			break;
		case MULTIBOOT_FRAMEBUFFER_TYPE_RGB:
			fb->font = get_default_font();
			if (!fb->font)
				return -ENOSPC;
			fb->width = fb->real_width / fb->font->width;
			fb->height = fb->real_height / fb->font->height;
			fb->update_region = fb_rgb_update_region;
			fb->scroll_up = fb_rgb_scroll_up;
			fb->scroll_down = fb_rgb_scroll_down;
			fb->update_cursor = fb_rgb_update_cursor;
			fb->show_cursor = fb_rgb_show_cursor;
			break;
		default:
			return -EINVAL;
	}

	/* allocate buffer */
	fb->buf = (uint16_t *) kmalloc(sizeof(uint16_t) * fb->width * (fb->height + 1));
	if (!fb->buf)
		return -ENOMEM;

	/* identity map frame buffer */
	fb_nb_pages = div_ceil(fb->real_height * fb->pitch, PAGE_SIZE);
	for (i = 0; i < fb_nb_pages; i++) {
		ret = map_page_phys(fb->addr + i * PAGE_SIZE, fb->addr + i * PAGE_SIZE, kernel_pgd, 0, 1);
		if (ret)
			goto err;
	}

	/* clear frame buffer */
	memsetw(fb->buf, erase_char, fb->width * (fb->height + 1));

	return 0;
err:
	kfree(fb->buf);
	return ret;
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
 * Open a frame buffer.
 */
static int fb_open(struct file_t *filp)
{
	UNUSED(filp);
	return 0;
}

/*
 * Read a framebuffer.
 */
static int fb_read(struct file_t *filp, char *buf, int n)
{
	UNUSED(filp);
	UNUSED(buf);
	UNUSED(n);
	return 0;
}

/*
 * Write to a frame buffer.
 */
static int fb_write(struct file_t *filp, const char *buf, int n)
{
	struct framebuffer_t *fb = &direct_fb;

	/* check position */
	if (filp->f_pos > fb->real_width * fb->real_height * fb->bpp / 8)
		return 0;

	/* ajust size */
	if (filp->f_pos + n > fb->real_width * fb->real_height * fb->bpp / 8)
		n = fb->real_width * fb->real_height * fb->bpp / 8 - filp->f_pos;

	/* write */
	memcpy((char *) (fb->addr + filp->f_pos), buf, n);

	/* update position */
	filp->f_pos += n;

	return n;
}

/*
 * Framebuffer file operations.
 */
static struct file_operations_t fb_fops = {
	.open	= fb_open,
	.read	= fb_read,
	.write	= fb_write,
};

/*
 * Framebuffer inode operations.
 */
struct inode_operations_t fb_iops = {
	.fops	= &fb_fops,
};
