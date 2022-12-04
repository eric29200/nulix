#include <drivers/console.h>
#include <drivers/keyboard.h>
#include <proc/sched.h>
#include <mm/mm.h>
#include <stdio.h>
#include <string.h>
#include <stderr.h>
#include <dev.h>
#include <kd.h>

/* consoles table */
static struct vc_t console_table[NR_CONSOLES];
int fg_console;

/* processes waiting for console activation */
struct wait_queue_t *vt_activate_wq = NULL;

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
 * Reset a virtual console.
 */
static void reset_vc(struct vc_t *vc)
{
	vc->vc_mode = KD_TEXT;
	kbd_table[vc->vc_num].kbd_mode = VC_XLATE;
	kbd_table[vc->vc_num].kbd_led_mode = LED_SHOW_FLAGS;
	kbd_table[vc->vc_num].kbd_led_flag_state = kbd_table[vc->vc_num].kbd_default_led_flag_state;
	vc->vt_mode.mode = VT_AUTO;
	vc->vt_mode.waitv = 0;
	vc->vt_mode.relsig = 0;
	vc->vt_mode.acqsig = 0;
	vc->vt_mode.frsig = 0;
	vc->vt_pid = -1;
	vc->vt_newvt = -1;
}

/*
 * Change current console.
 */
static void console_complete_change(int n)
{
	struct framebuffer_t *fb;
	struct vc_t *vc_new;

	/* check console */
	if (n < 0 || n >= NR_CONSOLES || n == fg_console)
		return;

	/* get current console */
	vc_new = &console_table[n];

	/* if new console is in process mode, acquire it */
	if (vc_new->vt_mode.mode == VT_PROCESS) {
		/* send acquire signal */
		if (task_signal(vc_new->vt_pid, vc_new->vt_mode.acqsig) != 0)
			reset_vc(vc_new);
	}

	/* disable current frame buffer */
	console_table[fg_console].fb.active = 0;

	/* refresh new frame buffer */
	if (vc_new->vc_mode == KD_TEXT) {
		fb = &vc_new->fb;
		fb->active = 1;
		fb->ops->update_region(fb, 0, fb->width * fb->height);
	}

	/* set current console */
	fg_console = n;

	/* wake up eventual processes */
	task_wakeup(&vt_activate_wq);
}

/*
 * Change current console.
 */
void console_change(int n)
{
	struct vc_t *vc;

	/* check console */
	if (n < 0 || n >= NR_CONSOLES || n == fg_console)
		return;

	/* get current console */
	vc = &console_table[fg_console];

	/* in process mode, handshake realase/acquire */
	if (vc->vt_mode.mode == VT_PROCESS) {
		/* send release signal */
		if (task_signal(vc->vt_pid, vc->vt_mode.relsig) == 0) {
			vc->vt_newvt = n;
			return;
		}

		/* on error reset console */
		reset_vc(vc);
	}

	/* ignore switches in KD_GRAPHICS mode */
	if (vc->vc_mode == KD_GRAPHICS)
		return;

	/* change console */
	console_complete_change(n);
}

/*
 * Default console attributes.
 */
static void console_default_attr(struct vc_t *vc)
{
	vc->vc_intensity = 1;
	vc->vc_reverse = 0;
	vc->vc_color = vc->vc_def_color;
}

/*
 * Update console attributes.
 */
static void console_update_attr(struct vc_t *vc)
{
	vc->vc_attr = vc->vc_color;

	if (vc->vc_reverse)
		vc->vc_attr = TEXT_COLOR(TEXT_COLOR_FG(vc->vc_color), TEXT_COLOR_BG(vc->vc_color));
	if (vc->vc_intensity == 2)
		vc->vc_attr ^= 0x08;

	/* redefine erase char */
	vc->vc_erase_char = ' ' | (vc->vc_color << 8);
}

/*
 * Scroll up from bottom to top.
 */
static void console_scrup(struct vc_t *vc, uint32_t top, uint32_t bottom, size_t nr)
{
	struct framebuffer_t *fb = &vc->fb;
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
	memsetw(dest + (bottom - top - nr) * fb->width, vc->vc_erase_char, fb->width * nr);

	/* hardware scroll */
	if (fb->active)
		fb->ops->scroll_up(fb, top, bottom, nr);
}

/*
 * Scroll down from top to bottom.
 */
static void console_scrdown(struct vc_t *vc, uint32_t top, uint32_t bottom, size_t nr)
{
	struct framebuffer_t *fb = &vc->fb;
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
	memsetw(src, vc->vc_erase_char, fb->width * nr);

	/* hardware scroll */
	if (fb->active)
		fb->ops->scroll_down(fb, top, bottom, nr);
}

/*
 * Insert a character at current position.
 */
static void insert_char(struct vc_t *vc)
{
	struct framebuffer_t *fb = &vc->fb;
	uint16_t *src, *dest;

	/* move each character of current line to right */
	src = (uint16_t *) (fb->buf + fb->y * fb->width + fb->x);
	dest = (uint16_t *) (fb->buf + fb->y * fb->width + fb->x + 1);
	memmovew(dest, src, fb->width - fb->x);

	/* update region */
	if (fb->active)
		fb->ops->update_region(fb, fb->y * fb->width + fb->x, fb->width - fb->x);
}

/*
 * Handle escape P sequences (delete characters).
 */
static void csi_P(struct vc_t *vc, uint32_t nr)
{
	struct framebuffer_t *fb = &vc->fb;
	uint16_t *p;

	if (nr > vc->fb.width - vc->fb.x)
		nr = vc->fb.width - vc->fb.x;
	else if (!nr)
		nr = 1;

	/* delete characters */
	p = fb->buf + fb->y * fb->width + fb->x;
	memcpy(p, p + nr, (fb->width - fb->x - nr) * 2);
	memsetw(p + fb->width - fb->x - nr, vc->vc_erase_char, nr);

	/* update region */
	if (fb->active)
		fb->ops->update_region(fb, fb->y * fb->width + fb->x, fb->width - fb->x);
}

/*
 * Handle escape M sequences (delete lines).
 */
static void csi_M(struct vc_t *vc, uint32_t nr)
{
	struct framebuffer_t *fb = &vc->fb;

	if (nr > vc->fb.height - vc->fb.y)
		nr = vc->fb.height - vc->fb.y;
	else if (!nr)
		nr = 1;

	/* scroll up */
	console_scrup(vc, fb->y, vc->vc_bottom, nr);
}

/*
 * Handle escape K sequences (erase line or part of line).
 */
static void csi_K(struct vc_t *vc, int vpar)
{
	struct framebuffer_t *fb = &vc->fb;
	uint16_t *start = fb->buf + fb->y * fb->width + fb->x;
	uint32_t count;
	int offset;

	switch (vpar) {
		case 0:						/* erase from cursor to end of line */
			offset = 0;
			count = vc->fb.width - vc->fb.x;
			break;
		case 1:						/* erase from start of line to cursor */
			offset = -vc->fb.x;
			count = vc->fb.x + 1;
			break;
		case 2:						/* erase whole line */
			offset = -vc->fb.x;
			count = vc->fb.width;
			break;
		default:
			return;
	}

	/* update frame buffer */
	memsetw(start + offset, vc->vc_erase_char, count);

	/* update region */
	if (fb->active)
		fb->ops->update_region(&vc->fb, start + offset - vc->fb.buf, count);
}

/*
 * Handle escape J sequences (delete screen or part of the screen).
 */
static void csi_J(struct vc_t *vc, int vpar)
{
	struct framebuffer_t *fb = &vc->fb;
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
	memsetw(start, vc->vc_erase_char, count);

	/* update region */
	if (fb->active)
		fb->ops->update_region(&vc->fb, start - vc->fb.buf, count);
}

/*
 * Handle escape m sequences (change console attributes).
 */
static void csi_m(struct vc_t *vc)
{
	size_t i;

	for (i = 0; i <= vc->vc_npars; i++) {
		switch (vc->vc_pars[i]) {
			case 0:												/* set default attributes */
				console_default_attr(vc);
				break;
			case 1:												/* bold */
				vc->vc_intensity = 2;
				break;
			case 4:												/* underline */
				vc->vc_underline = 1;
				break;
			case 7:												/* reverse attributes */
				vc->vc_reverse = 1;
				break;
			case 10:											/* set translation */
				vc->vc_translate = console_translations[vc->vc_charset == 0 ? vc->vc_charset_g0 : vc->vc_charset_g1];
				break;
			case 24:											/* not underline */
				vc->vc_underline = 0;
				break;
			case 27:											/* unreverse attributes */
				vc->vc_reverse = 0;
				break;
			case 39:											/* default foreground color */
				vc->vc_color = TEXT_COLOR(TEXT_COLOR_BG(vc->vc_color), TEXT_COLOR_FG(vc->vc_def_color));
				break;
			case 49:											/* default background color */
				vc->vc_color = TEXT_COLOR(TEXT_COLOR_BG(vc->vc_def_color), TEXT_COLOR_FG(vc->vc_color));
				break;
			default:
				/* set foreground color */
				if (vc->vc_pars[i] >= 30 && vc->vc_pars[i] <= 37) {
					vc->vc_color = TEXT_COLOR(TEXT_COLOR_BG(vc->vc_color), ansi_color_table[vc->vc_pars[i] - 30]);
					break;
				}

				/* set background color */
				if (vc->vc_pars[i] >= 40 && vc->vc_pars[i] <= 47) {
					vc->vc_color = TEXT_COLOR(ansi_color_table[vc->vc_pars[i] - 40], TEXT_COLOR_FG(vc->vc_color));
					break;
				}

				printf("console : unknown escape sequence m : %d\n", vc->vc_pars[i]);
				break;
		}
	}

	/* update attributes */
	console_update_attr(vc);
}

/*
 * Handle escape L sequences (scroll down).
 */
static void csi_L(struct vc_t *vc, uint32_t nr)
{
	struct framebuffer_t *fb = &vc->fb;

	if (nr > fb->height - fb->y)
		nr = fb->height - fb->y;
	else if (nr == 0)
		nr = 1;

	console_scrdown(vc, fb->y, fb->height, nr);
}

/*
 * Insert characters at current position.
 */
static void csi_at(struct vc_t *vc, uint32_t nr)
{
	struct framebuffer_t *fb = &vc->fb;

	if (nr > fb->width)
		nr = fb->width;
	else if (nr == 0)
		nr = 1;

	while (nr--)
		insert_char(vc);
}

/*
 * Handle escape X sequences (erase characters).
 */
static void csi_X(struct vc_t *vc, uint32_t nr)
{
	struct framebuffer_t *fb = &vc->fb;

	if (nr == 0)
		nr = 1;
	if (nr > fb->width - fb->x)
		nr = fb->width - fb->x;

	/* update frame buffer */
	memsetw(fb->buf + fb->y * fb->width + fb->x, vc->vc_erase_char, nr);

	/* update region */
	if (fb->active)
		fb->ops->update_region(&vc->fb, fb->y * fb->width + fb->x, nr);
}

/*
 * Set console mode.
 */
static void console_set_mode(struct vc_t *vc, int on_off)
{
	size_t i;

	for (i = 0; i <= vc->vc_npars; i++) {
		switch (vc->vc_pars[i]) {
			case 25:				/* cursor visible */
				vc->vc_deccm = on_off;
				if (vc->fb.active)
					vc->fb.ops->show_cursor(&vc->fb, on_off);
				break;
			case 2004:				/* bracket mode = ignore */
				break;
			default:
				printf("console : unknown mode : %d\n", vc->vc_pars[i]);
				break;
		}
	}
}

/*
 * Do a carriage return.
 */
static void console_cr(struct vc_t *vc)
{
	vc->fb.x = 0;
	vc->vc_need_wrap = 0;
}

/*
 * Do a line feed.
 */
static void console_lf(struct vc_t *vc)
{
	struct framebuffer_t *fb = &vc->fb;

	if (fb->y + 1 == vc->vc_bottom)
		console_scrup(vc, vc->vc_top, vc->vc_bottom, 1);
	else if (fb->y < fb->height - 1)
		fb->y++;

	vc->vc_need_wrap = 0;
}

/*
 * Scroll down if needed.
 */
static void console_ri(struct vc_t *vc)
{
	struct framebuffer_t *fb = &vc->fb;

	/* don't scroll if not needed */
	if (fb->y == vc->vc_top)
		console_scrdown(vc, vc->vc_top, vc->vc_bottom, 1);
	else if (fb->y > 0)
		fb->y--;

	vc->vc_need_wrap = 0;
}

/*
 * Do a back space.
 */
static void console_bs(struct vc_t *vc)
{
	if (vc->fb.x) {
		vc->fb.x--;
		vc->vc_need_wrap = 0;
	}
}

/*
 * Print a character on the console.
 */
static void console_putc(struct vc_t *vc, uint8_t c)
{
	struct framebuffer_t *fb = &vc->fb;
	uint16_t tc;

	/* wrap if needed */
	if (vc->vc_need_wrap) {
		console_cr(vc);
		console_lf(vc);
	}

	/* translate character */
	tc = vc->vc_translate[c];

	/* add character */
	fb->buf[fb->y * fb->width + fb->x] = (vc->vc_attr << 8) + tc;
	if (fb->active)
		fb->ops->update_region(fb, fb->y * fb->width + fb->x, 1);

	/* update position */
	if (fb->x == fb->width - 1)
		vc->vc_need_wrap = 1;
	else
		fb->x++;
}

/*
 * Go to (x ; y).
 */
static void console_gotoxy(struct vc_t *vc, uint32_t x, uint32_t y)
{
	struct framebuffer_t *fb = &vc->fb;

	if (x >= fb->width)
		fb->x = fb->width - 1;
	else
		fb->x = x;

	if (y >= fb->height)
		fb->y = fb->height - 1;
	else
		fb->y = y;

	vc->vc_need_wrap = 0;
}

/*
 * Respond a string to tty.
 */
static void console_respond_string(struct tty_t *tty, char *p)
{
	while (*p) {
		ring_buffer_putc(&tty->read_queue, *p);
		p++;
	}
}

/*
 * Report console cursor.
 */
static void console_cursor_report(struct tty_t *tty, struct vc_t *vc)
{
	char buf[40];

	sprintf(buf, "\033[%d;%dR", vc->fb.y + 1, vc->fb.x + 1);
	console_respond_string(tty, buf);
}

/*
 * Report console status.
 */
static void console_status_report(struct tty_t *tty)
{
	console_respond_string(tty, "\033[0n");	/* Terminal ok */
}

/*
 * Handle control characters.
 */
static void console_do_control(struct tty_t *tty, struct vc_t *vc, uint8_t c)
{
	struct framebuffer_t *fb = &vc->fb;

	/* handle special character */
	switch (c) {
		case 8:
			console_bs(vc);
			return;
		case 9:
			fb->x = (fb->x + fb->bpp / 8) & ~0x03;
			return;
		case 10:
		case 11:
		case 12:
			console_lf(vc);
			console_cr(vc);
			return;
		case 13:
			console_cr(vc);
			return;
		case 27:
			vc->vc_state = TTY_STATE_ESCAPE;
			return;
	}

	/* handle escape sequence */
	if (vc->vc_state == TTY_STATE_ESCAPE) {
			vc->vc_state = TTY_STATE_NORMAL;

			switch (c) {
				case '[':
					vc->vc_state = TTY_STATE_SQUARE;
					break;
				case 'M':
					console_ri(vc);
					break;
				case '(':
					vc->vc_state = TTY_STATE_SET_G0;
					break;
				case ')':
					vc->vc_state = TTY_STATE_SET_G1;
					break;
				default:
					printf("console : unknown escape sequence %c\n", c);
					break;
			}

			return;
	}

	/* set g0 charset */
	if (vc->vc_state == TTY_STATE_SET_G0) {
		switch (c) {
			case '0':
				vc->vc_charset_g0 = GRAF_MAP;
				break;
			case 'B':
				vc->vc_charset_g0 = LAT1_MAP;
				break;
			case 'U':
				vc->vc_charset_g0 = IBMPC_MAP;
				break;
			case 'K':
				vc->vc_charset_g0 = USER_MAP;
				break;
		}

		if (vc->vc_charset == 0)
			vc->vc_translate = console_translations[vc->vc_charset_g0];

		vc->vc_state = TTY_STATE_NORMAL;
		return;
	}

	/* set g1 charset */
	if (vc->vc_state == TTY_STATE_SET_G1) {
		switch (c) {
			case '0':
				vc->vc_charset_g1 = GRAF_MAP;
				break;
			case 'B':
				vc->vc_charset_g1 = LAT1_MAP;
				break;
			case 'U':
				vc->vc_charset_g1 = IBMPC_MAP;
				break;
			case 'K':
				vc->vc_charset_g1 = USER_MAP;
				break;
		}

		if (vc->vc_charset == 1)
			vc->vc_translate = console_translations[vc->vc_charset_g1];

		vc->vc_state = TTY_STATE_NORMAL;
		return;
	}

	/* handle escape sequence */
	if (vc->vc_state == TTY_STATE_SQUARE) {
		/* reset npars */
		for (vc->vc_npars = 0; vc->vc_npars < NPARS; vc->vc_npars++)
			vc->vc_pars[vc->vc_npars] = 0;
		vc->vc_npars = 0;

		/* get pars */
		vc->vc_state = TTY_STATE_GETPARS;

		/* question ? */
		vc->vc_ques = (c == '?');
		if (vc->vc_ques)
			return;
	}

	/* get pars */
	if (vc->vc_state == TTY_STATE_GETPARS) {
		if (c == ';' && vc->vc_npars < NPARS - 1) {
			vc->vc_npars++;
			return;
		}

		if (c >= '0' && c <= '9') {
			vc->vc_pars[vc->vc_npars] *= 10;
			vc->vc_pars[vc->vc_npars] += c - '0';
			return;
		}

		vc->vc_state = TTY_STATE_GOTPARS;
	}

	/* handle pars */
	if (vc->vc_state == TTY_STATE_GOTPARS) {
		vc->vc_state = TTY_STATE_NORMAL;

		/* question sequences */
		switch (c) {
			case 'h':
				console_set_mode(vc, 1);
				return;
			case 'l':
				console_set_mode(vc, 0);
				return;
			case 'c':
				if (vc->vc_ques)
					return;
				break;
			case 'm':
				if (vc->vc_ques)
					return;
				break;
			case 'n':
				if (!vc->vc_ques) {
					if (vc->vc_pars[0] == 5)
						console_status_report(tty);
					else if (vc->vc_pars[0] == 6)
						console_cursor_report(tty, vc);
				}

				return;
		}

		/* reset question */
		if (vc->vc_ques) {
			vc->vc_ques = 0;
			return;
		}

		switch (c) {
			case 'G':
				if (vc->vc_pars[0])
					vc->vc_pars[0]--;
				console_gotoxy(vc, vc->vc_pars[0], vc->fb.y);
				break;
			case 'A':
				if (!vc->vc_pars[0])
					vc->vc_pars[0]++;
				console_gotoxy(vc, vc->fb.x, vc->fb.y - vc->vc_pars[0]);
				break;
			case 'B':
				if (!vc->vc_pars[0])
					vc->vc_pars[0]++;
				console_gotoxy(vc, vc->fb.x, vc->fb.y + vc->vc_pars[0]);
				break;
			case 'C':
				if (!vc->vc_pars[0])
					vc->vc_pars[0]++;
				console_gotoxy(vc, vc->fb.x + vc->vc_pars[0], vc->fb.y);
				break;
			case 'D':
				if (!vc->vc_pars[0])
					vc->vc_pars[0]++;
				console_gotoxy(vc, vc->fb.x - vc->vc_pars[0], vc->fb.y);
				break;
			case 'd':
				if (vc->vc_pars[0])
					vc->vc_pars[0]--;
				console_gotoxy(vc, vc->fb.x, vc->vc_pars[0]);
				break;
			case 'H':
				if (vc->vc_pars[0])
					vc->vc_pars[0]--;
				if (vc->vc_pars[1])
					vc->vc_pars[1]--;
				console_gotoxy(vc, vc->vc_pars[1], vc->vc_pars[0]);
				break;
			case 'r':
				if (!vc->vc_pars[0])
					vc->vc_pars[0]++;
				if (!vc->vc_pars[1])
					vc->vc_pars[1] = vc->fb.height;
				if (vc->vc_pars[0] < vc->vc_pars[1] && vc->vc_pars[1] <= vc->fb.height) {
					vc->vc_top = vc->vc_pars[0] - 1;
					vc->vc_bottom = vc->vc_pars[1];
					console_gotoxy(vc, 0, 0);
				}
				break;
			case 'P':
				csi_P(vc, vc->vc_pars[0]);
				break;
			case 'M':
				csi_M(vc, vc->vc_pars[0]);
				break;
			case 'K':
				csi_K(vc, vc->vc_pars[0]);
				break;
			case 'J':
				csi_J(vc, vc->vc_pars[0]);
				break;
			case 'm':
				csi_m(vc);
				break;
			case 'L':
				csi_L(vc, vc->vc_pars[0]);
				break;
			case '@':
				csi_at(vc, vc->vc_pars[0]);
				break;
			case 'X':
				csi_X(vc, vc->vc_pars[0]);
				break;
			case 'c':
				break;
			default:
				printf("console : unknown escape sequence %c (gotpars)\n", c);
				break;
		}
	}
}

/*
 * Write to TTY.
 */
static ssize_t console_write(struct tty_t *tty)
{
	ssize_t count = 0;
	struct vc_t *vc;
	uint8_t c;

	/* get console */
	vc = tty->driver_data;

	/* remove cursor */
	if (vc->fb.active)
		vc->fb.ops->update_region(&vc->fb, vc->fb.cursor_y * vc->fb.width + vc->fb.cursor_x, 1);

	/* get characters from write queue */
	while (tty->write_queue.size > 0) {
		/* get next character */
		ring_buffer_read(&tty->write_queue, &c, 1);
		count++;

		/* just put new character */
		if (vc->vc_state == TTY_STATE_NORMAL && c >= 32) {
			console_putc(vc, c);
			continue;
		}

		/* do control */
		console_do_control(tty, vc, c);
	}

	/* update cursor */
	if (vc->vc_deccm && vc->fb.active)
		vc->fb.ops->update_cursor(&vc->fb);

	return count;
}

/*
 * Wait for a console activation.
 */
static int vt_waitactive(int n)
{
	for (;;) {
		/* vt == current tty : exit */
		if (n == fg_console)
			break;

		/* pending signals : exit */
		if (signal_pending(current_task))
			return -EINTR;

		/* sleep on waiting queue */
		task_sleep(&vt_activate_wq);
	}

	return 0;
}
/*
 * Console ioctl.
 */
static int console_ioctl(struct tty_t *tty, int request, unsigned long arg)
{
	int i, new_func_len, old_func_len, newvt;
	uint16_t *key_map, mask;
	struct kbsentry_t *kbse;
	struct vt_stat *vtstat;
	struct kbentry_t *kbe;
	struct kbd_t *kbd;
	struct vc_t *vc;
	char *old_func;

	/* get console and keyboard */
	vc = tty->driver_data;
	kbd = &kbd_table[vc->vc_num];

	switch (request) {
		case KDGKBTYPE:
			*((char *) arg) = KB_101;
			return 0;
		case KDGETMODE:
			*((uint8_t *) arg) = vc->vc_mode;
			return 0;
		case KDSETMODE:
			vc->vc_mode = *((uint8_t *) arg);
			return 0;
		case KDGKBENT:
			kbe = (struct kbentry_t *) arg;
			if (!kbe)
				return -EINVAL;

			/* get key entry */
			key_map = key_maps[kbe->kb_table];
			if (key_map) {
				kbe->kb_value = U(key_map[kbe->kb_index]);
				if (KTYP(kbe->kb_value) >= NR_TYPES)
					kbe->kb_value = K_HOLE;
			} else if (kbe->kb_index) {
				kbe->kb_value = K_HOLE;
			} else {
				kbe->kb_value = K_NOSUCHMAP;
			}

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
			new_func_len = strlen((char *) kbse->kb_string);

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
		case KDGKBMODE:
			switch(kbd->kbd_mode) {
				  case VC_RAW:
					*((int *) arg) = K_RAW;
					break;
				  case VC_MEDIUMRAW:
					*((int *) arg) = K_MEDIUMRAW;
					break;
				  case VC_XLATE:
					*((int *) arg) = K_XLATE;
					break;
				  case VC_UNICODE:
					*((int *) arg) = K_UNICODE;
					break;
				  default:
					break;
			}
			return 0;
		case KDSKBMODE:
			switch(arg) {
				  case K_RAW:
					kbd->kbd_mode = VC_RAW;
					break;
				  case K_MEDIUMRAW:
					kbd->kbd_mode = VC_MEDIUMRAW;
					break;
				  case K_XLATE:
					kbd->kbd_mode = VC_XLATE;
					break;
				  case K_UNICODE:
					kbd->kbd_mode = VC_UNICODE;
					break;
				  default:
					return -EINVAL;
			}
			return 0;
		case KDGKBLED:
			*((uint8_t *) arg) = kbd->kbd_led_flag_state | (kbd->kbd_default_led_flag_state << 4);
			return 0;
		case KDSKBLED:
			if (arg & ~0x77)
				return -EINVAL;
			kbd->kbd_led_flag_state = (arg & 7);
			kbd->kbd_default_led_flag_state = ((arg >> 4) & 7);
			return 0;
		case VT_GETSTATE:
			vtstat = (struct vt_stat *) arg;
			vtstat->v_active = fg_console + 1;
			for (i = 0, mask = 2, vtstat->v_state = 1; i < NR_CONSOLES; i++, mask <<= 1)
				vtstat->v_state |= mask;
			return 0;
		case VT_GETMODE:
			*((struct vt_mode *) arg) = vc->vt_mode;
			return 0;
		case VT_SETMODE:
			vc->vt_mode = *((struct vt_mode *) arg);
			vc->vt_mode.frsig = 0;
			vc->vt_pid = current_task->pid;
			vc->vt_newvt = -1;
			return 0;
		case VT_ACTIVATE:
			if (arg == 0 || arg > NR_CONSOLES)
				return -ENXIO;
			console_change(arg - 1);
			return 0;
		case VT_RELDISP:
			if (vc->vt_mode.mode != VT_PROCESS)
				return -EINVAL;

			/* change console */
			if (vc->vt_newvt >= 0) {
				if (arg == 0) {
					vc->vt_newvt = -1;
					return 0;
				}

				newvt = vc->vt_newvt;
				vc->vt_newvt = -1;
				console_complete_change(newvt);
			} else if (arg != VT_ACKACQ) {
				return -EINVAL;
			}

			return 0;
		case VT_WAITACTIVE:
			if (arg == 0 || arg > NR_CONSOLES)
				return -ENXIO;

			/* wait for console activation */
			return vt_waitactive(arg - 1);
		case VT_OPENQRY:
			/* find free tty */
		 	for (i = 0; i < NR_CONSOLES; i++)
				if (tty_table[i].count == 0)
					break;

			*((int *) arg) = i < NR_CONSOLES ? i + 1 : -1;
			return 0;
		default:
			break;
	}

	return -ENOIOCTLCMD;
}

/*
 * Console driver.
 */
static struct tty_driver_t console_driver = {
	.write		= console_write,
	.ioctl		= console_ioctl,
	.termios 	= (struct termios_t) {
				.c_iflag	= ICRNL | IXON,
				.c_oflag	= OPOST | ONLCR,
				.c_cflag	= B38400 | CS8 | CREAD | HUPCL,
				.c_lflag	= ISIG | ICANON | ECHO | ECHOE | ECHOK | ECHOCTL | ECHOKE | IEXTEN,
				.c_line		= 0,
				.c_cc		= INIT_C_CC,
			},
};

/*
 * Init consoles.
 */
int init_console(struct multiboot_tag_framebuffer *tag_fb)
{
	struct tty_t *tty;
	struct vc_t *vc;
	int i, ret;

	/* init consoles */
	for (i = 0; i < NR_CONSOLES; i++) {
		tty = &tty_table[i];
		vc = &console_table[i];

		/* init tty device */
		ret = tty_init_dev(tty, &console_driver);
		if (ret)
			goto err;

		/* init console attributes */
		vc->vc_num = i;
		vc->vc_def_color = TEXT_COLOR(TEXT_BLACK, TEXT_LIGHT_GREY);
		vc->vc_color = vc->vc_def_color;
		vc->vc_intensity = 1;
		vc->vc_reverse = 0;
		vc->vc_erase_char = ' ' | (vc->vc_def_color << 8);
		vc->vc_deccm = 1;
		vc->vc_attr = vc->vc_color;
		vc->vc_translate = console_translations[LAT1_MAP];
		reset_vc(vc);

		/* init frame buffer */
		ret = init_framebuffer(&vc->fb, tag_fb, vc->vc_erase_char, 0);
		if (ret)
			goto err;

		/* set winsize */
		tty->winsize.ws_row = vc->fb.height;
		tty->winsize.ws_col = vc->fb.width;
		vc->vc_top = 0;
		vc->vc_bottom = vc->fb.height;

		/* attach console to tty */
		tty->driver_data = vc;

	}

	/* set current console to first console */
	fg_console = 0;
	console_table[fg_console].fb.active = 1;

	return 0;
err:
	/* destroy consoles */
	for (i = 0; i < NR_CONSOLES; i++)
		tty_destroy(&tty_table[i]);

	return ret;
}
