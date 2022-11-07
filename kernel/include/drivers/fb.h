#ifndef _FB_H_
#define _FB_H_

#include <grub/multiboot2.h>
#include <fs/fs.h>
#include <stddef.h>
#include <lib/font.h>
#include <string.h>

#define TEXT_BLACK			0
#define TEXT_LIGHT_GREY			7
#define TEXT_COLOR(bg, fg)		(((bg) << 4) | (fg))
#define TEXT_COLOR_BG(color)		(((color) & 0xF0) >> 4)
#define TEXT_COLOR_FG(color)		((color) & 0x0F)
#define TEXT_ENTRY(c, color)		(((color) << 8) | (c))

/* ioctls */
#define FBIOGET_VSCREENINFO		0x4600
#define FBIOPUT_VSCREENINFO		0x4601
#define FBIOGET_FSCREENINFO		0x4602
#define FBIOGETCMAP			0x4604
#define FBIOPUTCMAP			0x4605
#define FBIOPAN_DISPLAY			0x4606

#define FB_TYPE_PACKED_PIXELS		0	/* Packed Pixels */
#define FB_TYPE_PLANES			1	/* Non interleaved planes */
#define FB_TYPE_INTERLEAVED_PLANES	2	/* Interleaved planes */
#define FB_TYPE_TEXT			3	/* Text/attributes */
#define FB_TYPE_VGA_PLANES		4	/* EGA/VGA planes */

#define FB_AUX_TEXT_MDA			0	/* Monochrome text */
#define FB_AUX_TEXT_CGA			1	/* CGA/EGA/VGA Color text */
#define FB_AUX_TEXT_S3_MMIO		2	/* S3 MMIO fasttext */
#define FB_AUX_TEXT_MGA_STEP16		3	/* MGA Millenium I: text, attr, 14 reserved bytes */
#define FB_AUX_TEXT_MGA_STEP8		4	/* other MGAs:      text, attr,  6 reserved bytes */

#define FB_VISUAL_MONO01		0	/* Monochr. 1=Black 0=White */
#define FB_VISUAL_MONO10		1	/* Monochr. 1=White 0=Black */
#define FB_VISUAL_TRUECOLOR		2	/* True color */
#define FB_VISUAL_PSEUDOCOLOR		3	/* Pseudo color (like atari) */
#define FB_VISUAL_DIRECTCOLOR		4	/* Direct color */
#define FB_VISUAL_STATIC_PSEUDOCOLOR	5	/* Pseudo color readonly */

struct framebuffer_t;

/*
 * Framebuffer fix informations.
 */
struct fb_fix_screeninfo {
	char		id[16];			/* identification string eg "TT Builtin" */
	unsigned long	smem_start;		/* Start of frame buffer mem (physical address) */
	uint32_t	smem_len;		/* Length of frame buffer mem */
	uint32_t	type;			/* see FB_TYPE_* */
	uint32_t	type_aux;		/* Interleave for interleaved Planes */
	uint32_t	visual;			/* see FB_VISUAL_* */
	uint16_t	xpanstep;		/* zero if no hardware panning */
	uint16_t	ypanstep;		/* zero if no hardware panning */
	uint16_t	ywrapstep;		/* zero if no hardware ywrap */
	uint32_t	line_length;		/* length of a line in bytes */
	unsigned long	mmio_start;		/* Start of Memory Mapped I/O (physical address) */
	uint32_t	mmio_len;		/* Length of Memory Mapped I/O  */
	uint32_t	accel;			/* Indicate to driver which specific chip/card we have	*/
	uint16_t	capabilities;		/* see FB_CAP_* */
	uint16_t	reserved[2];		/* Reserved for future compatibility */
};

/*
 * Framebuffer bit field.
 */
struct fb_bitfield {
	uint32_t 	offset;			/* beginning of bitfield */
	uint32_t 	length;			/* length of bitfield */
	uint32_t 	msb_right;		/* != 0 : Most significant bit is right */
};

/*
 * Framebuffer variable informations.
 */
struct fb_var_screeninfo {
	uint32_t	 	xres;			/* visible resolution */
	uint32_t		yres;
	uint32_t		xres_virtual;		/* virtual resolution */
	uint32_t		yres_virtual;
	uint32_t		xoffset;		/* offset from virtual to visible */
	uint32_t		yoffset;		/* resolution */
	uint32_t		bits_per_pixel;		/* guess what */
	uint32_t		grayscale;		/* 0 = color, 1 = grayscale,  >1 = FOURCC */
	struct fb_bitfield	red;			/* bitfield in fb mem if true color, */
	struct fb_bitfield	green;			/* else only length is significant */
	struct fb_bitfield	blue;
	struct fb_bitfield	transp;			/* transparency */
	uint32_t		nonstd;			/* != 0 Non standard pixel format */
	uint32_t		activate;		/* see FB_ACTIVATE_* */
	uint32_t		height;			/* height of picture in mm */
	uint32_t		width;			/* width of picture in mm */
	uint32_t		accel_flags;		/* (OBSOLETE) see fb_info.flags */
	/* Timing: All values in pixclocks, except pixclock (of course) */
	uint32_t		pixclock;		/* pixel clock in ps (pico seconds) */
	uint32_t		left_margin;		/* time from sync to picture */
	uint32_t		right_margin;		/* time from picture to sync */
	uint32_t		upper_margin;		/* time from sync to picture */
	uint32_t		lower_margin;
	uint32_t		hsync_len;		/* length of horizontal sync */
	uint32_t		vsync_len;		/* length of vertical sync */
	uint32_t		sync;			/* see FB_SYNC_* */
	uint32_t		vmode;			/* see FB_VMODE_* */
	uint32_t		rotate;			/* angle we rotate counter clockwise */
	uint32_t		colorspace;		/* colorspace for FOURCC-based modes */
	uint32_t		reserved[4];		/* Reserved for future compatibility */
};

/*
 * Frame buffer operations.
 */
struct framebuffer_ops_t {
	int			(*init)(struct framebuffer_t *);
	void			(*update_region)(struct framebuffer_t *, uint32_t, uint32_t);
	void			(*scroll_up)(struct framebuffer_t *, uint32_t, uint32_t, size_t);
	void			(*scroll_down)(struct framebuffer_t *, uint32_t, uint32_t, size_t);
	void			(*update_cursor)(struct framebuffer_t *);
	void			(*show_cursor)(struct framebuffer_t *, int);
	int			(*get_fix)(struct framebuffer_t *, struct fb_fix_screeninfo *);
	int			(*get_var)(struct framebuffer_t *, struct fb_var_screeninfo *);
};

/*
 * Frame buffer structure.
 */
struct framebuffer_t {
	uint32_t			addr;
	uint16_t			type;
	uint32_t			pitch;
	uint32_t			width;
	uint32_t			height;
	uint32_t			real_width;
	uint32_t			real_height;
	uint8_t				bpp;
	struct font_t *			font;
	uint32_t			x;
	uint32_t			y;
	uint32_t			cursor_x;
	uint32_t			cursor_y;
	uint16_t *			buf;
	int				active;
	struct framebuffer_ops_t	*ops;
};

int init_framebuffer(struct framebuffer_t *fb, struct multiboot_tag_framebuffer *tag_fb, uint16_t erase_char, int direct);
int init_framebuffer_direct(struct multiboot_tag_framebuffer *tag_fb);

/* frame buffers */
extern struct framebuffer_ops_t fb_text_ops;
extern struct framebuffer_ops_t fb_rgb_ops;

/* frame buffer inode operations */
extern struct inode_operations_t fb_iops;

#endif
