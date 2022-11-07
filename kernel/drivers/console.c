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
struct tty_t console_table[NR_CONSOLES];
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
static void reset_vc(struct tty_t *tty)
{
	tty->vc_mode = KD_TEXT;
	kbd_table[tty->dev - DEV_TTY0 - 1].kbdmode = VC_XLATE;
	tty->vt_mode.mode = VT_AUTO;
	tty->vt_mode.waitv = 0;
	tty->vt_mode.relsig = 0;
	tty->vt_mode.acqsig = 0;
	tty->vt_mode.frsig = 0;
	tty->vt_pid = -1;
	tty->vt_newvt = -1;
}

/*
 * Change current console.
 */
static void console_complete_change(int n)
{
	struct framebuffer_t *fb;
	struct tty_t *tty_new;

	/* check tty */
	if (n < 0 || n >= NR_CONSOLES || n == fg_console)
		return;

	/* get current tty */
	tty_new = &console_table[n];

	/* if new console is in process mode, acquire it */
	if (tty_new->vt_mode.mode == VT_PROCESS) {
		/* send acquire signal */
		if (task_signal(tty_new->vt_pid, tty_new->vt_mode.acqsig) != 0)
			reset_vc(tty_new);
	}

	/* disable current frame buffer */
	console_table[fg_console].fb.active = 0;

	/* refresh new frame buffer */
	if (tty_new->vc_mode == KD_TEXT) {
		fb = &tty_new->fb;
		fb->active = 1;
		fb->ops->update_region(fb, 0, fb->width * fb->height);
	}

	/* set current tty */
	fg_console = n;

	/* wake up eventual processes */
	task_wakeup(&vt_activate_wq);
}

/*
 * Change current console.
 */
void console_change(int n)
{
	struct tty_t *tty;

	/* check tty */
	if (n < 0 || n >= NR_CONSOLES || n == fg_console)
		return;

	/* get current tty */
	tty = &console_table[fg_console];

	/* in process mode, handshake realase/acquire */
	if (tty->vt_mode.mode == VT_PROCESS) {
		/* send release signal */
		if (task_signal(tty->vt_pid, tty->vt_mode.relsig) == 0) {
			tty->vt_newvt = n;
			return;
		}

		/* on error reset console */
		reset_vc(tty);
	}

	/* ignore switches in KD_GRAPHICS mode */
	if (tty->vc_mode == KD_GRAPHICS)
		return;

	/* change console */
	console_complete_change(n);
}

/*
 * Init console attributes.
 */
static void console_init_attr(struct tty_t *tty)
{
	tty->vc_def_color = TEXT_COLOR(TEXT_BLACK, TEXT_LIGHT_GREY);
	tty->vc_color = tty->vc_def_color;
	tty->vc_intensity = 1;
	tty->vc_reverse = 0;
	tty->vc_erase_char = ' ' | (tty->vc_def_color << 8);
	tty->vc_deccm = 1;
	tty->vc_attr = tty->vc_color;
}

/*
 * Default console attributes.
 */
static void console_default_attr(struct tty_t *tty)
{
	tty->vc_intensity = 1;
	tty->vc_reverse = 0;
	tty->vc_color = tty->vc_def_color;
}

/*
 * Update console attributes.
 */
static void console_update_attr(struct tty_t *tty)
{
	tty->vc_attr = tty->vc_color;

	if (tty->vc_reverse)
		tty->vc_attr = TEXT_COLOR(TEXT_COLOR_FG(tty->vc_color), TEXT_COLOR_BG(tty->vc_color));
	if (tty->vc_intensity == 2)
		tty->vc_attr ^= 0x08;

	/* redefine erase char */
	tty->vc_erase_char = ' ' | (tty->vc_color << 8);
}

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
	memsetw(dest + (bottom - top - nr) * fb->width, tty->vc_erase_char, fb->width * nr);

	/* hardware scroll */
	if (fb->active)
		fb->ops->scroll_up(fb, top, bottom, nr);
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
	memsetw(src, tty->vc_erase_char, fb->width * nr);

	/* hardware scroll */
	if (fb->active)
		fb->ops->scroll_down(fb, top, bottom, nr);
}

/*
 * Insert a character at current position.
 */
static void insert_char(struct tty_t *tty)
{
	struct framebuffer_t *fb = &tty->fb;
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
	memsetw(p + fb->width - fb->x - nr, tty->vc_erase_char, nr);

	/* update region */
	if (fb->active)
		fb->ops->update_region(fb, fb->y * fb->width + fb->x, fb->width - fb->x);
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
	memsetw(start + offset, tty->vc_erase_char, count);

	/* update region */
	if (fb->active)
		fb->ops->update_region(&tty->fb, start + offset - tty->fb.buf, count);
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
	memsetw(start, tty->vc_erase_char, count);

	/* update region */
	if (fb->active)
		fb->ops->update_region(&tty->fb, start - tty->fb.buf, count);
}

/*
 * Handle escape m sequences (change console attributes).
 */
static void csi_m(struct tty_t *tty)
{
	size_t i;

	for (i = 0; i <= tty->vc_npars; i++) {
		switch (tty->vc_pars[i]) {
			case 0:												/* set default attributes */
				console_default_attr(tty);
				break;
			case 1:												/* bold */
				tty->vc_intensity = 2;
				break;
			case 4:												/* underline */
				tty->vc_underline = 1;
				break;
			case 7:												/* reverse attributes */
				tty->vc_reverse = 1;
				break;
			case 24:											/* not underline */
				tty->vc_underline = 0;
				break;
			case 27:											/* unreverse attributes */
				tty->vc_reverse = 0;
				break;
			case 39:											/* default foreground color */
				tty->vc_color = TEXT_COLOR(TEXT_COLOR_BG(tty->vc_color), TEXT_COLOR_FG(tty->vc_def_color));
				break;
			case 49:											/* default background color */
				tty->vc_color = TEXT_COLOR(TEXT_COLOR_BG(tty->vc_def_color), TEXT_COLOR_FG(tty->vc_color));
				break;
			default:
				/* set foreground color */
				if (tty->vc_pars[i] >= 30 && tty->vc_pars[i] <= 37) {
					tty->vc_color = TEXT_COLOR(TEXT_COLOR_BG(tty->vc_color), ansi_color_table[tty->vc_pars[i] - 30]);
					break;
				}

				/* set background color */
				if (tty->vc_pars[i] >= 40 && tty->vc_pars[i] <= 47) {
					tty->vc_color = TEXT_COLOR(ansi_color_table[tty->vc_pars[i] - 40], TEXT_COLOR_FG(tty->vc_color));
					break;
				}

				printf("console : unknown escape sequence m : %d\n", tty->vc_pars[i]);
				break;
		}
	}

	/* update attributes */
	console_update_attr(tty);
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
 * Insert characters at current position.
 */
static void csi_at(struct tty_t *tty, uint32_t nr)
{
	struct framebuffer_t *fb = &tty->fb;

	if (nr > fb->width)
		nr = fb->width;
	else if (nr == 0)
		nr = 1;

	insert_char(tty);
}

/*
 * Set console mode.
 */
static void console_set_mode(struct tty_t *tty, int on_off)
{
	size_t i;

	for (i = 0; i <= tty->vc_npars; i++) {
		switch (tty->vc_pars[i]) {
			case 25:				/* cursor visible */
				tty->vc_deccm = on_off;
				if (tty->fb.active)
					tty->fb.ops->show_cursor(&tty->fb, on_off);
				break;
			default:
				printf("console : unknown mode : %d\n", tty->vc_pars[i]);
				break;
		}
	}
}

/*
 * Do a carriage return.
 */
static void console_cr(struct tty_t *tty)
{
	tty->fb.x = 0;
	tty->vt_need_wrap = 0;
}

/*
 * Do a line feed.
 */
static void console_lf(struct tty_t *tty)
{
	struct framebuffer_t *fb = &tty->fb;

	if (fb->y + 1 == fb->height)
		console_scrup(tty, 0, fb->height, 1);
	else if (fb->y < fb->height - 1)
		fb->y++;

	tty->vt_need_wrap = 0;
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

	tty->vt_need_wrap = 0;
}

/*
 * Do a back space.
 */
static void console_bs(struct tty_t *tty)
{
	if (tty->fb.x) {
		tty->fb.x--;
		tty->vt_need_wrap = 0;
	}
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
		case 8:
			console_bs(tty);
			break;
		case 9:
			fb->x = (fb->x + fb->bpp / 8) & ~0x03;
			break;
		case 10:
		case 11:
		case 12:
			console_lf(tty);
			console_cr(tty);
			break;
		case 13:
			console_cr(tty);
			break;
		case 14:
			break;
		case 15:
			break;
		default:
			/* wrap if needed */
			if (tty->vt_need_wrap) {
				console_cr(tty);
				console_lf(tty);
			}

			/* add character */
			fb->buf[fb->y * fb->width + fb->x] = (tty->vc_attr << 8) | c;
			if (fb->active)
				fb->ops->update_region(fb, fb->y * fb->width + fb->x, 1);

			/* update position */
			if (fb->x == fb->width - 1)
				tty->vt_need_wrap = 1;
			else
				fb->x++;

			break;
	}
}

/*
 * Go to (x ; y).
 */
static void console_gotoxy(struct tty_t *tty, uint32_t x, uint32_t y)
{
	struct framebuffer_t *fb = &tty->fb;

	if (x >= fb->width)
		fb->x = fb->width - 1;
	else
		fb->x = x;

	if (y >= fb->height)
		fb->y = fb->height - 1;
	else
		fb->y = y;

	tty->vt_need_wrap = 0;
}

/*
 * Write to TTY.
 */
static ssize_t console_write(struct tty_t *tty)
{
	ssize_t count = 0;
	uint8_t c;

	/* remove cursor */
	if (tty->fb.active)
		tty->fb.ops->update_region(&tty->fb, tty->fb.cursor_y * tty->fb.width + tty->fb.cursor_x, 1);

	/* get characters from write queue */
	while (tty->write_queue.size > 0) {
		/* get next character */
		ring_buffer_read(&tty->write_queue, &c, 1);
		count++;

		/* start an escape sequence or write to frame buffer */
		if (tty->vc_state == TTY_STATE_NORMAL) {
				switch (c) {
					case '\033':
						tty->vc_state = TTY_STATE_ESCAPE;
						break;
					default:
						console_putc(tty, c);
						break;
				}

				continue;
		}

		/* handle escape sequence */
		if (tty->vc_state == TTY_STATE_ESCAPE) {
				tty->vc_state = TTY_STATE_NORMAL;

				switch (c) {
					case '[':
						tty->vc_state = TTY_STATE_SQUARE;
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
		if (tty->vc_state == TTY_STATE_SQUARE) {
			/* reset npars */
			for (tty->vc_npars = 0; tty->vc_npars < NPARS; tty->vc_npars++)
				tty->vc_pars[tty->vc_npars] = 0;
			tty->vc_npars = 0;

			tty->vc_state = TTY_STATE_GETPARS;
			if (c == '?')
				continue;
		}

		/* get pars */
		if (tty->vc_state == TTY_STATE_GETPARS) {
			if (c == ';' && tty->vc_npars < NPARS - 1) {
				tty->vc_npars++;
				continue;
			}

			if (c >= '0' && c <= '9') {
				tty->vc_pars[tty->vc_npars] *= 10;
				tty->vc_pars[tty->vc_npars] += c - '0';
				continue;
			}

			tty->vc_state = TTY_STATE_GOTPARS;
		}

		/* handle pars */
		if (tty->vc_state == TTY_STATE_GOTPARS) {
			tty->vc_state = TTY_STATE_NORMAL;

			switch (c) {
				case 'G':
					if (tty->vc_pars[0])
						tty->vc_pars[0]--;
					console_gotoxy(tty, tty->vc_pars[0], tty->fb.y);
					break;
				case 'A':
					if (!tty->vc_pars[0])
						tty->vc_pars[0]++;
					console_gotoxy(tty, tty->fb.x, tty->fb.y - tty->vc_pars[0]);
					break;
				case 'B':
					if (!tty->vc_pars[0])
						tty->vc_pars[0]++;
					console_gotoxy(tty, tty->fb.x, tty->fb.y + tty->vc_pars[0]);
					break;
				case 'C':
					if (!tty->vc_pars[0])
						tty->vc_pars[0]++;
					console_gotoxy(tty, tty->fb.x + tty->vc_pars[0], tty->fb.y);
					break;
				case 'D':
					if (!tty->vc_pars[0])
						tty->vc_pars[0]++;
					console_gotoxy(tty, tty->fb.x - tty->vc_pars[0], tty->fb.y);
					break;
				case 'd':
					if (tty->vc_pars[0])
						tty->vc_pars[0]--;
					console_gotoxy(tty, tty->fb.x, tty->vc_pars[0]);
					break;
				case 'H':
					if (tty->vc_pars[0])
						tty->vc_pars[0]--;
					if (tty->vc_pars[1])
						tty->vc_pars[1]--;
					console_gotoxy(tty, tty->vc_pars[1], tty->vc_pars[0]);
					break;
				case 'r':
					if (!tty->vc_pars[0])
						tty->vc_pars[0]++;
					if (!tty->vc_pars[1])
						tty->vc_pars[1] = tty->fb.height;
					if (tty->vc_pars[0] < tty->vc_pars[1] && tty->vc_pars[1] <= tty->fb.height)
						console_gotoxy(tty, 0, 0);
					break;
				case 'P':
					csi_P(tty, tty->vc_pars[0]);
					break;
				case 'K':
					csi_K(tty, tty->vc_pars[0]);
					break;
				case 'J':
					csi_J(tty, tty->vc_pars[0]);
					break;
				case 'm':
					csi_m(tty);
					break;
				case 'L':
					csi_L(tty, tty->vc_pars[0]);
					break;
				case '@':
					csi_at(tty, tty->vc_pars[0]);
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
	if (tty->vc_deccm && tty->fb.active)
		tty->fb.ops->update_cursor(&tty->fb);

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
		if (!sigisemptyset(&current_task->sigpend))
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
	char *old_func;

	/* get keyboard */
	kbd = &kbd_table[tty->dev - DEV_TTY0 - 1];

	switch (request) {
		case KDGKBTYPE:
			*((char *) arg) = KB_101;
			return 0;
		case KDGETMODE:
			*((uint8_t *) arg) = tty->vc_mode;
			return 0;
		case KDSETMODE:
			tty->vc_mode = *((uint8_t *) arg);
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
			switch(kbd->kbdmode) {
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
					kbd->kbdmode = VC_RAW;
					break;
				  case K_MEDIUMRAW:
					kbd->kbdmode = VC_MEDIUMRAW;
					break;
				  case K_XLATE:
					kbd->kbdmode = VC_XLATE;
					break;
				  case K_UNICODE:
					kbd->kbdmode = VC_UNICODE;
					break;
				  default:
					return -EINVAL;
			}
			return 0;
		case VT_GETSTATE:
			vtstat = (struct vt_stat *) arg;
			vtstat->v_active = fg_console + 1;
			for (i = 0, mask = 2, vtstat->v_state = 1; i < NR_CONSOLES; i++, mask <<= 1)
				vtstat->v_state |= mask;
			return 0;
		case VT_GETMODE:
			*((struct vt_mode *) arg) = tty->vt_mode;
			return 0;
		case VT_SETMODE:
			tty->vt_mode = *((struct vt_mode *) arg);
			tty->vt_mode.frsig = 0;
			tty->vt_pid = current_task->pid;
			tty->vt_newvt = -1;
			return 0;
		case VT_ACTIVATE:
			if (arg == 0 || arg > NR_CONSOLES)
				return -ENXIO;
			console_change(arg - 1);
			return 0;
		case VT_RELDISP:
			if (tty->vt_mode.mode != VT_PROCESS)
				return -EINVAL;

			/* change tty */
			if (tty->vt_newvt >= 0) {
				if (arg == 0) {
					tty->vt_newvt = -1;
					return 0;
				}

				newvt = tty->vt_newvt;
				tty->vt_newvt = -1;
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
};

/*
 * Init consoles.
 */
int init_console(struct multiboot_tag_framebuffer *tag_fb)
{
	struct tty_t *tty;
	int i, ret;

	/* reset consoles */
	for (i = 0; i < NR_CONSOLES; i++)
		memset(&console_table[i], 0, sizeof(struct tty_t));

	/* init consoles */
	for (i = 0; i < NR_CONSOLES; i++) {
		tty = &console_table[i];

		/* init tty device */
		ret = tty_init_dev(tty, DEV_TTY0 + i + 1, &console_driver);
		if (ret)
			goto err;

		/* init frame buffer */
		ret = init_framebuffer(&tty->fb, tag_fb, tty->vc_erase_char, 0);
		if (ret)
			goto err;

		/* set winsize */
		tty->winsize.ws_row = tty->fb.height;
		tty->winsize.ws_col = tty->fb.width;

		/* init console attributes */
		console_init_attr(tty);
		reset_vc(tty);
	}

	/* set current tty to first tty */
	fg_console = 0;
	console_table[fg_console].fb.active = 1;

	return 0;
err:
	/* destroy consoles */
	for (i = 0; i < NR_CONSOLES; i++)
		tty_destroy(&console_table[i]);

	return ret;
}
