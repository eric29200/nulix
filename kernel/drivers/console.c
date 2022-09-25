#include <drivers/console.h>
#include <drivers/keyboard.h>
#include <mm/mm.h>
#include <stdio.h>
#include <string.h>
#include <stderr.h>
#include <kd.h>

/* ansi color table */
static uint8_t ansi_color_table[] = {
	0,	/* black */
	4,	/* blue */
	2,	/* green */
	6,	/* cyan */
	1,	/* red */
	5,	/* magenta */
	3,	/* brown */
	7,	/* light gray */
	8,	/* dark gray */
	12,	/* light blue */
	10,	/* light green */
	14,	/* light cyan */
	9,	/* light red */
	13,	/* light magenta */
	11,	/* yellow */
	15	/* white */
};

/*
 * Scroll up from bottom to top.
 */
static void console_scrup(struct tty_t *tty, uint32_t top, uint32_t bottom, size_t nr)
{
	struct framebuffer_t *fb = &tty->fb;
	uint16_t *dest, *src;

	/* limit to bottom */
	if (top + nr >= bottom)
		nr = bottom - top - 1;

	/* check top and bottom */
	if (bottom > fb->height || top >= bottom || nr < 1)
		return;

	/* move each line up */
	dest = (uint16_t *) (fb->buf + fb->width * top);
	src = (uint16_t *) (fb->buf + fb->width * (top + nr));
	memmovew(dest, src, (bottom - top - nr) * fb->width);

	/* clear last lines */
	memsetw(dest + (bottom - top - nr) * fb->width, tty->erase_char, fb->width * nr);

	/* hardware scroll */
	if (fb->active)
		fb->scroll_up(fb, top, bottom, nr);
}

/*
 * Scroll down from top to bottom.
 */
static void console_scrdown(struct tty_t *tty, uint32_t top, uint32_t bottom, size_t nr)
{
	struct framebuffer_t *fb = &tty->fb;
	uint16_t *src, *dest;

	/* limit to bottom */
	if (top + nr >= bottom)
		nr = bottom - top - 1;

	/* check top and bottom */
	if (bottom > fb->height || top >= bottom || nr < 1)
		return;

	/* move each line down */
	dest = (uint16_t *) (fb->buf + fb->width * (top + nr));
	src = (uint16_t *) (fb->buf + fb->width * top);
	memmovew(dest, src, (bottom - top - nr) * fb->width);

	/* clear first lines */
	memsetw(src, tty->erase_char, fb->width * nr);

	/* hardware scroll */
	if (fb->active)
		fb->scroll_down(fb, top, bottom, nr);
}

/*
 * Handle escape P sequences (delete characters).
 */
static void csi_P(struct tty_t *tty, uint32_t nr)
{
	struct framebuffer_t *fb = &tty->fb;
	uint16_t *p;

	if (nr > tty->fb.width - tty->fb.x)
		nr = tty->fb.width - tty->fb.x;
	else if (!nr)
		nr = 1;

	/* delete characters */
	p = fb->buf + fb->y * fb->width + fb->x;
	memcpy(p, p + nr, (fb->width - fb->x - nr) * 2);
	memsetw(p + fb->width - fb->x - nr, tty->erase_char, nr);

	/* update region */
	if (fb->active)
		fb->update_region(fb, fb->y * fb->width + fb->x, fb->width - fb->x);
}

/*
 * Handle escape K sequences (erase line or part of line).
 */
static void csi_K(struct tty_t *tty, int vpar)
{
	struct framebuffer_t *fb = &tty->fb;
	uint16_t *start = fb->buf + fb->y * fb->width + fb->x;
	uint32_t count;
	int offset;

	switch (vpar) {
		case 0:						/* erase from cursor to end of line */
			offset = 0;
			count = tty->fb.width - tty->fb.x;
			break;
		case 1:						/* erase from start of line to cursor */
			offset = -tty->fb.x;
			count = tty->fb.x + 1;
			break;
		case 2:						/* erase whole line */
			offset = -tty->fb.x;
			count = tty->fb.width;
			break;
		default:
			return;
	}

	/* update frame buffer */
	memsetw(start + offset, tty->erase_char, count);

	/* update region */
	if (fb->active)
		fb->update_region(&tty->fb, start + offset - tty->fb.buf, count);
}

/*
 * Handle escape J sequences (delete screen or part of the screen).
 */
static void csi_J(struct tty_t *tty, int vpar)
{
	struct framebuffer_t *fb = &tty->fb;
	uint16_t *start;
	size_t count;

	switch (vpar) {
		case 0:								/* erase from cursor to end of display */
			start = fb->buf + fb->y * fb->width + fb->x;
			count = fb->width * fb->height - fb->y * fb->width + fb->x;
			break;
		case 1:								/* erase from start of display to cursor */
			start = fb->buf;
			count = fb->y * fb->width + fb->x;
			break;
		case 2:								/* erase whole display */
			start = fb->buf;
			count = fb->width * fb->height;
			break;
		default:
			break;
	}

	/* update frame buffer */
	memsetw(start, tty->erase_char, count);

	/* update region */
	if (fb->active)
		fb->update_region(&tty->fb, start - tty->fb.buf, count);
}

/*
 * Handle escape m sequences (change console attributes).
 */
static void csi_m(struct tty_t *tty)
{
	size_t i;

	for (i = 0; i <= tty->npars; i++) {
		switch (tty->pars[i]) {
			case 0:												/* set default attributes */
				tty_default_attr(tty);
				break;
			case 1:												/* bold */
				tty->intensity = 2;
				break;
			case 7:												/* reverse attributes */
				tty->reverse = 1;
				break;
			case 27:											/* unreverse attributes */
				tty->reverse = 0;
				break;
			case 39:											/* default foreground color */
				tty->color = TEXT_COLOR(TEXT_COLOR_BG(tty->color), TEXT_COLOR_FG(tty->def_color));
				break;
			case 49:											/* default background color */
				tty->color = TEXT_COLOR(TEXT_COLOR_BG(tty->def_color), TEXT_COLOR_FG(tty->color));
				break;
			default:
				/* set foreground color */
				if (tty->pars[i] >= 30 && tty->pars[i] <= 37) {
					tty->color = TEXT_COLOR(TEXT_COLOR_BG(tty->color), ansi_color_table[tty->pars[i] - 30]);
					break;
				}

				/* set background color */
				if (tty->pars[i] >= 40 && tty->pars[i] <= 47) {
					tty->color = TEXT_COLOR(ansi_color_table[tty->pars[i] - 40], TEXT_COLOR_FG(tty->color));
					break;
				}

				printf("console : unknown escape sequence m : %d\n", tty->pars[i]);
				break;
		}
	}

	/* update attributes */
	tty_update_attr(tty);
}

/*
 * Handle escape L sequences (scroll down).
 */
static void csi_L(struct tty_t *tty, uint32_t nr)
{
	struct framebuffer_t *fb = &tty->fb;

	if (nr > fb->height - fb->y)
		nr = fb->height - fb->y;
	else if (nr == 0)
		nr = 1;

	console_scrdown(tty, fb->y, fb->height, nr);
}

/*
 * Set console mode.
 */
static void console_set_mode(struct tty_t *tty, int on_off)
{
	size_t i;

	for (i = 0; i <= tty->npars; i++) {
		switch (tty->pars[i]) {
			case 25:				/* cursor visible */
				tty->deccm = on_off;
				if (tty->fb.active)
					tty->fb.show_cursor(&tty->fb, on_off);
				break;
			default:
				printf("console : unknown mode : %d\n", tty->pars[i]);
				break;
		}
	}
}

/*
 * Scroll down if needed.
 */
static void console_ri(struct tty_t *tty)
{
	struct framebuffer_t *fb = &tty->fb;

	/* don't scroll if not needed */
	if (fb->y == 0)
		console_scrdown(tty, 0, fb->height, 1);
	else if (fb->y > 0)
		fb->y--;
}

/*
 * Print a character on the console.
 */
static void console_putc(struct tty_t *tty, uint8_t c)
{
	struct framebuffer_t *fb = &tty->fb;

	/* handle character */
	switch (c) {
		case 7:
			break;
		case '\t':
			fb->x = (fb->x + fb->bpp / 8) & ~0x03;
			break;
		case '\n':
			fb->y++;
			fb->x = 0;
			break;
		case '\r':
			fb->x = 0;
			break;
		case '\b':
			fb->x--;
			break;
		default:
			fb->buf[fb->y * fb->width + fb->x] = (tty->attr << 8) | c;
			if (fb->active)
				fb->update_region(fb, fb->y * fb->width + fb->x, 1);
			fb->x++;
			break;
	}

	/* go to next line */
	if (fb->x >= fb->width) {
		fb->x = 0;
		fb->y++;
	}

	/* scroll */
	if (fb->y >= fb->height) {
		/* scroll up */
		console_scrup(tty, 0, fb->height, 1);

		/* update position */
		fb->y = fb->height - 1;
	}
}

/*
 * Write to TTY.
 */
void console_write(struct tty_t *tty)
{
	uint8_t c;

	/* remove cursor */
	if (tty->fb.active)
		tty->fb.update_region(&tty->fb, tty->fb.cursor_y * tty->fb.width + tty->fb.cursor_x, 1);

	/* get characters from write queue */
	while (tty->write_queue.size > 0) {
		/* get next character */
		ring_buffer_read(&tty->write_queue, &c, 1);

		/* start an escape sequence or write to frame buffer */
		if (tty->state == TTY_STATE_NORMAL) {
				switch (c) {
					case '\033':
						tty->state = TTY_STATE_ESCAPE;
						break;
					default:
						console_putc(tty, c);
						break;
				}

				continue;
		}

		/* handle escape sequence */
		if (tty->state == TTY_STATE_ESCAPE) {
				tty->state = TTY_STATE_NORMAL;

				switch (c) {
					case '[':
						tty->state = TTY_STATE_SQUARE;
						break;
					case 'M':
						console_ri(tty);
						break;
					default:
						printf("console : unknown escape sequence %c\n", c);
						break;
				}

				continue;
		}

		/* handle escape sequence */
		if (tty->state == TTY_STATE_SQUARE) {
			/* reset npars */
			for (tty->npars = 0; tty->npars < NPARS; tty->npars++)
				tty->pars[tty->npars] = 0;
			tty->npars = 0;

			tty->state = TTY_STATE_GETPARS;
			if (c == '?')
				continue;
		}

		/* get pars */
		if (tty->state == TTY_STATE_GETPARS) {
			if (c == ';' && tty->npars < NPARS - 1) {
				tty->npars++;
				continue;
			}

			if (c >= '0' && c <= '9') {
				tty->pars[tty->npars] *= 10;
				tty->pars[tty->npars] += c - '0';
				continue;
			}

			tty->state = TTY_STATE_GOTPARS;
		}

		/* handle pars */
		if (tty->state == TTY_STATE_GOTPARS) {
			tty->state = TTY_STATE_NORMAL;

			switch (c) {
				case 'G':
					if (tty->pars[0])
						tty->pars[0]--;
					fb_set_xy(&tty->fb, tty->pars[0], tty->fb.y);
					break;
				case 'A':
					if (!tty->pars[0])
						tty->pars[0]++;
					fb_set_xy(&tty->fb, tty->fb.x, tty->fb.y - tty->pars[0]);
					break;
				case 'C':
					if (!tty->pars[0])
						tty->pars[0]++;
					fb_set_xy(&tty->fb, tty->fb.x + tty->pars[0], tty->fb.y);
					break;
				case 'd':
					if (tty->pars[0])
						tty->pars[0]--;
					fb_set_xy(&tty->fb, tty->fb.x, tty->pars[0]);
					break;
				case 'H':
					if (tty->pars[0])
						tty->pars[0]--;
					if (tty->pars[1])
						tty->pars[1]--;
					fb_set_xy(&tty->fb, tty->pars[1], tty->pars[0]);
					break;
				case 'r':
					if (!tty->pars[0])
						tty->pars[0]++;
					if (!tty->pars[1])
						tty->pars[1] = tty->fb.height;
					if (tty->pars[0] < tty->pars[1] && tty->pars[1] <= tty->fb.height)
						fb_set_xy(&tty->fb, 0, 0);
					break;
				case 'P':
					csi_P(tty, tty->pars[0]);
					break;
				case 'K':
					csi_K(tty, tty->pars[0]);
					break;
				case 'J':
					csi_J(tty, tty->pars[0]);
					break;
				case 'm':
					csi_m(tty);
					break;
				case 'L':
					csi_L(tty, tty->pars[0]);
					break;
				case 'h':
					console_set_mode(tty, 1);
					break;
				case 'l':
					console_set_mode(tty, 0);
					break;
				case 'c':
					break;
				default:
					printf("console : unknown escape sequence %c (gotpars)\n", c);
					break;
			}

			continue;
		}
	}

	/* update cursor */
	if (tty->deccm && tty->fb.active)
		tty->fb.update_cursor(&tty->fb);
}

/*
 * Console ioctl.
 */
int console_ioctl(struct tty_t *tty, int request, unsigned long arg)
{
	int i, new_func_len, old_func_len;
	struct kbsentry_t *kbse;
	struct kbentry_t *kbe;
	uint16_t *key_map;
	char *old_func;

	/* unused tty */
	UNUSED(tty);

	switch (request) {
		case KDGKBTYPE:
			*((char *) arg) = KB_101;
			return 0;
		case KDSKBENT:
			/* check keyboard entry */
			kbe = (struct kbentry_t *) arg;
			if (!kbe)
				return -EINVAL;
			if (KTYP(kbe->kb_value) >= NR_TYPES)
				return -EINVAL;
			if (KVAL(kbe->kb_value) > max_vals[KTYP(kbe->kb_value)])
				return -EINVAL;

			/* get or create key_map */
			key_map = key_maps[kbe->kb_table];
			if (!key_map) {
				key_map = (uint16_t *) kmalloc(sizeof(plain_map));
				if (!key_map)
					return -ENOMEM;
				key_maps[kbe->kb_table] = key_map;
				key_map[0] = U(K_ALLOCATED);
				for (i = 1; i < NR_KEYS; i++)
					key_map[i] = U(K_HOLE);
			}

		 	/* set key */
			key_maps[kbe->kb_table][kbe->kb_index] = U(kbe->kb_value);
			return 0;
		case KDSKBSENT:
			/* get function entry */
			kbse = (struct kbsentry_t *) arg;
			if (!kbse)
				return -EINVAL;

			/* get functions */
			old_func = (char *) func_table[kbse->kb_func];
			old_func_len = old_func ? strlen(old_func) : 0;
			new_func_len = kbse->kb_string ? strlen((char *) kbse->kb_string) : 0;

			/* free old entry */
			if (old_func_len && (new_func_len == 0 || new_func_len > old_func_len)
			    && func_table[kbse->kb_func] >= (uint8_t *) KHEAP_START)
				kfree(func_table[kbse->kb_func]);

			/* set new function */
			if (new_func_len == 0)
				func_table[kbse->kb_func] = NULL;
			else if (old_func_len >= new_func_len)
				strcpy((char *) func_table[kbse->kb_func], (char *) kbse->kb_string);
			else
				func_table[kbse->kb_func] = (uint8_t *) strdup((char *) kbse->kb_string);

			return 0;
		default:
			break;
	}

	return -ENOIOCTLCMD;
}
