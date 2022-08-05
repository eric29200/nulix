#include <drivers/console.h>
#include <stdio.h>
#include <string.h>

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
 * Set console mode.
 */
static void console_set_mode(struct tty_t *tty, int on_off)
{
	size_t i;

	for (i = 0; i <= tty->npars; i++) {
		switch (tty->pars[i]) {
			case 25:				/* cursor visible */
				tty->deccm = on_off;
				tty->fb.show_cursor(&tty->fb, on_off);
				break;
			default:
				printf("console : unknown mode : %d\n", tty->pars[i]);
				break;
		}
	}
}

/*
 * Print a character on the console.
 */
static void console_putc(struct tty_t *tty, uint8_t c)
{
	struct framebuffer_t *fb = &tty->fb;

	/* handle character */
	if (c >= ' ' && c <= '~') {
		fb->buf[fb->y * fb->width + fb->x] = (tty->attr << 8) | c;
		fb->update_region(fb, fb->y * fb->width + fb->x, 1);
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
		fb->y++;
	}

	/* scroll */
	if (fb->y >= fb->height) {
		/* move each line up */
		memcpy(fb->buf, fb->buf + fb->width, fb->width * (fb->height - 1));

		/* clear last line */
		memsetw(fb->buf + fb->width * (fb->height - 1), tty->erase_char, fb->width);

		/* hardware scroll */
		fb->scroll(fb);

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
				switch (c) {
					case '[':
						tty->state = TTY_STATE_SQUARE;
						break;
					default:
						tty->state = TTY_STATE_NORMAL;
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
				case 'h':
					console_set_mode(tty, 1);
					break;
				case 'l':
					console_set_mode(tty, 0);
					break;
				default:
					printf("console : unknown escape sequence %c\n", c);
					break;
			}

			continue;
		}
	}

	/* update cursor */
	if (tty->deccm)
		tty->fb.update_cursor(&tty->fb);
}
