#include <drivers/console.h>
#include <stdio.h>
#include <string.h>

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
	memset(p + fb->width - fb->x - nr, 0, nr * 2);
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
	memset(start + offset, 0, count * 2);
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
	memset(start, 0, count * 2);
	tty->fb.dirty = 1;
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
						fb_putc(&tty->fb, chars[i]);
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
