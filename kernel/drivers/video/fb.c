#include <drivers/video/fb.h>
#include <mm/mm.h>
#include <mm/paging.h>
#include <mm/mmap.h>
#include <proc/sched.h>
#include <sys/syscall.h>
#include <fcntl.h>
#include <string.h>
#include <stderr.h>
#include <stdio.h>
#include <dev.h>

/* direct frame buffer */
static struct framebuffer direct_fb;

/*
 * Init the framebuffer.
 */
int init_framebuffer(struct framebuffer *fb, struct multiboot_tag_framebuffer *tag_fb, uint16_t erase_char, int direct, int cursor_on)
{
	int ret;

	/* set frame buffer */
	fb->addr = tag_fb->common.framebuffer_addr;
	fb->type = tag_fb->common.framebuffer_type;
	fb->pitch = tag_fb->common.framebuffer_pitch;
	fb->real_width = tag_fb->common.framebuffer_width;
	fb->real_height = tag_fb->common.framebuffer_height;
	fb->bpp = tag_fb->common.framebuffer_bpp;
	fb->tag_fb = tag_fb;
	fb->x = fb->cursor_x = 0;
	fb->y = fb->cursor_y = 0;
	fb->cursor_on = cursor_on;
	fb->active = 0;

	/* init frame buffer */
	switch (fb->type) {
		case MULTIBOOT_FRAMEBUFFER_TYPE_EGA_TEXT:
			fb->ops = &fb_text_ops;
			break;
		case MULTIBOOT_FRAMEBUFFER_TYPE_RGB:
			fb->ops = &fb_rgb_ops;
			break;
		default:
			return -EINVAL;
	}

	/* init framebuffer */
	ret = fb->ops->init(fb);
	if (ret)
		return ret;

	/* direct frame buffer */
	if (direct == 1)
		return 0;

	/* allocate buffer */
	fb->buf = (uint16_t *) kmalloc(sizeof(uint16_t) * fb->width * (fb->height + 1));
	if (!fb->buf)
		return -ENOMEM;

	/* clear frame buffer */
	memsetw(fb->buf, erase_char, fb->width * (fb->height + 1));

	return 0;
}

/*
 * Read a framebuffer.
 */
static int fb_read(struct file *filp, char *buf, size_t n, off_t *ppos)
{
	UNUSED(filp);
	UNUSED(buf);
	UNUSED(n);
	UNUSED(ppos);
	return 0;
}

/*
 * Write to a frame buffer.
 */
static int fb_write(struct file *filp, const char *buf, size_t n, off_t *ppos)
{
	struct framebuffer *fb = &direct_fb;

	/* unused file */
	UNUSED(filp);

	/* check position */
	if (*ppos > fb->real_width * fb->real_height * fb->bpp / 8)
		return 0;

	/* ajust size */
	if (*ppos + n > fb->real_width * fb->real_height * fb->bpp / 8)
		n = fb->real_width * fb->real_height * fb->bpp / 8 - *ppos;

	/* write */
	memcpy((char *) fb->addr + *ppos, buf, n);

	/* update position */
	*ppos += n;

	return n;
}

/*
 * Framebuffer ioctl.
 */
int fb_ioctl(struct inode *inode, struct file *filp, int request, unsigned long arg)
{
	struct framebuffer *fb = &direct_fb;
	int ret = 0;

	UNUSED(filp);

	switch (request) {
		case FBIOGET_FSCREENINFO:
			if (!fb->ops || !fb->ops->get_fix)
				return -EINVAL;

			ret = fb->ops->get_fix(fb, (struct fb_fix_screeninfo *) arg);
			break;
		case FBIOGET_VSCREENINFO:
			if (!fb->ops || !fb->ops->get_var)
				return -EINVAL;

			ret = fb->ops->get_var(fb, (struct fb_var_screeninfo *) arg);
			break;
		case FBIOPUT_VSCREENINFO:
			ret = fb->ops->put_var(fb, (struct fb_var_screeninfo *) arg);
			break;
		default:
			printf("Unknown ioctl request (0x%x) on device 0x%x\n", request, inode->i_rdev);
			break;
	}

	return ret;
}

/*
 * Framebuffer mmap.
 */
static int fb_mmap(struct file *filp, struct vm_area *vma)
{
	struct inode *inode = filp->f_dentry->d_inode;
	struct framebuffer *fb = &direct_fb;
	int ret;

	/* offset must be page aligned */
	if (vma->vm_offset & (PAGE_SIZE - 1))
		return -EINVAL;

	/* update inode */
	update_atime(inode);

	/* remap frame buffer */
	ret = remap_page_range(vma->vm_start, fb->addr + vma->vm_offset, vma->vm_end - vma->vm_start, PAGE_SHARED);
	if (ret)
		return ret;

	return 0;
}

/*
 * Framebuffer file operations.
 */
static struct file_operations fb_fops = {
	.read		= fb_read,
	.write		= fb_write,
	.ioctl		= fb_ioctl,
	.mmap		= fb_mmap,
};

/*
 * Init framebuffer.
 */
int init_framebuffer_device(struct multiboot_tag_framebuffer *tag_fb)
{
	int ret;

	/* identity map frame buffer */
	ret = remap_page_range(tag_fb->common.framebuffer_addr,
		tag_fb->common.framebuffer_addr,
		tag_fb->common.framebuffer_height * tag_fb->common.framebuffer_pitch,
		PAGE_SHARED);
	if (ret)
		return ret;

	/* register direct framebuffer device */
	ret = register_chrdev(DEV_FB_MAJOR, "fb", &fb_fops);
	if (ret)
		return ret;

	/* init direct framebuffer */
	return init_framebuffer(&direct_fb, tag_fb, 0, 1, 0);
}
