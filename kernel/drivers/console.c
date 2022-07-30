#include <drivers/console.h>
#include <stdio.h>

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
