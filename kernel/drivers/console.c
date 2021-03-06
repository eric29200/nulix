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
	fb->dirty = 1;
}

/*
 * Handle escape K sequences (erase line or part of line).
 */
static void csi_K(struct tty_t *tty, int vpar)
{
	uint16_t *start = tty->fb.buf + tty->fb.y * tty->fb.width + tty->fb.x;
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
	tty->fb.dirty = 1;
}

/*
 * Handle escape J sequences (delete screen or part of the screen).
 */
static void csi_J(struct tty_t *tty, int vpar)
{
	uint16_t *start;
	size_t count;

	switch (vpar) {
		case 0:								/* erase from cursor to end of display */
			start = tty->fb.buf + tty->fb.y * tty->fb.width + tty->fb.x;
			count = tty->fb.width * tty->fb.height - tty->fb.y * tty->fb.width + tty->fb.x;
			break;
		case 1:								/* erase from start of display to cursor */
			start = tty->fb.buf;
			count = tty->fb.y * tty->fb.width + tty->fb.x;
			break;
		case 2:								/* erase whole display */
			start = tty->fb.buf;
			count = tty->fb.width * tty->fb.height;
			break;
		default:
			break;
	}

	/* update frame buffer */
	memsetw(start, tty->erase_char, count);
	tty->fb.dirty = 1;
}

/*
 * Handle escape m sequences (change console attributes).
 */
static void csi_m(struct tty_t *tty)
{
	size_t i;

	for (i = 0; i <= tty->npars; i++) {
		switch (tty->pars[i]) {
			case 0:								/* set default attributes */
				tty_default_attr(tty);
				break;
			default:
				/* set foreground color */
				if (tty->pars[i] >= 30 && tty->pars[i] <= 37) {
					tty->color = TEXT_COLOR(tty->color_bg, ansi_color_table[tty->pars[i] - 30]);
					break;
				}

				printf("console : unknown escape sequence m : %d\n", tty->pars[i]);
				break;
		}
	}
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
static void console_putc(struct tty_t *tty, uint8_t c, uint8_t color)
{
	struct framebuffer_t *fb = &tty->fb;
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
		memsetw(&fb->buf[i], tty->erase_char, fb->width * sizeof(uint16_t));

		fb->y = fb->height - 1;
	}

	/* mark frame buffer dirty */
	fb->dirty = 1;
}

/*
 * Write to TTY.
 */
int console_write(struct tty_t *tty, const char *buf, int n)
{
	const char *chars;
	int i;

	/* parse characters */
	chars = (const char *) buf;
	for (i = 0; i < n; i++) {
		/* start an escape sequence or write to frame buffer */
		if (tty->state == TTY_STATE_NORMAL) {
				switch (chars[i]) {
					case '\033':
						tty->state = TTY_STATE_ESCAPE;
						break;
					default:
						console_putc(tty, chars[i], tty->color);
						break;
				}

				continue;
		}

		/* handle escape sequence */
		if (tty->state == TTY_STATE_ESCAPE) {
				switch (chars[i]) {
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
			if (chars[i] == '?')
				continue;
		}

		/* get pars */
		if (tty->state == TTY_STATE_GETPARS) {
			if (chars[i] == ';' && tty->npars < NPARS - 1) {
				tty->npars++;
				continue;
			}

			if (chars[i] >= '0' && chars[i] <= '9') {
				tty->pars[tty->npars] *= 10;
				tty->pars[tty->npars] += chars[i] - '0';
				continue;
			}

			tty->state = TTY_STATE_GOTPARS;
		}

		/* handle pars */
		if (tty->state == TTY_STATE_GOTPARS) {
			tty->state = TTY_STATE_NORMAL;

			switch (chars[i]) {
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
					printf("console : unknown escape sequence %c\n", chars[i]);
					break;
			}

			continue;
		}
	}

	tty->fb.dirty = 1;
	return n;
}
