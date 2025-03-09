#include <drivers/char/console_selection.h>
#include <drivers/char/keyboard.h>
#include <stdio.h>
#include <stderr.h>

#define isspace(c)		((c) == ' ')
#define atedge(vc, p)		(!(p % (vc)->fb.width) || !((p + 1) % - (vc)->fb.width))

/* current selection */
static int sel_start = -1;
static int sel_end;
int sel_cons = 0;
static char *sel_buf = NULL;
static int sel_buf_len = 0;

/*
 * User settable table: what characters are to be considered alphabetic?
 * 256 bits
 */
static uint32_t inwordLut[8] = {
	0x00000000, /* control chars     */
	0x03FF0000, /* digits            */
	0x87FFFFFE, /* uppercase and '_' */
	0x07FFFFFE, /* lowercase         */
	0x00000000,
	0x00000000,
	0xFF7FFFFF, /* latin-1 accented letters, not multiplication sign */
	0xFF7FFFFF  /* latin-1 accented letters, not division sign */
};

/*
 * Character in word ?
 */
static inline int inword(const uint8_t c)
{
	return (inwordLut[c >> 5] >> (c & 0x1F) ) & 1;
}

/*
 * Set inwordlut contents.
 */
static int sel_loadlut(unsigned long arg)
{
	memcpy(inwordLut, (uint32_t *) (arg + 4), 32);
	return 0;
}

/*
 * Hilight pointer.
 */
static void highlight_pointer(struct vc *vc, int pos)
{
	static uint32_t old_pos = 0;
	static uint16_t old_tc = 0;
	static int old_update = 0;

	/* unhilight old pointer */
	if (old_update) {
		vc->fb.buf[old_pos] = old_tc;
		if (vc->fb.active)
			vc->fb.ops->update_region(&vc->fb, old_pos, 1);
	}

	/* don't highlight new pointer */
	if (pos == -1) {
		old_update = 0;
		return;
	}

	/* save current pointer position */
	old_pos = pos;
	old_tc = vc->fb.buf[old_pos];
	old_update = 1;

	/* hilight new pointer */
	vc->fb.buf[old_pos] = old_tc ^ 0x7700;
	if (vc->fb.active)
		vc->fb.ops->update_region(&vc->fb, old_pos, 1);
}

/*
 * Hilight.
 */
static void hilight(struct vc *vc, int start, int end)
{
	int i;

	/* limit to end of frame buffer */
	if ((uint32_t) end >= vc->fb.width * vc->fb.height)
		end = vc->fb.width * vc->fb.height - 1;

	/* revert selection */
	for (i = start; i < end + 1; i++)
		vc->fb.buf[i] ^= 0x7700;

	/* update frame buffer */
	if (vc->fb.active)
		vc->fb.ops->update_region(&vc->fb, start, end - start + 1);
}

/*
 * Clear selection.
 */
void clear_selection(struct vc *vc)
{
	/* hide pointer */
	highlight_pointer(vc, -1);

	/* clear selection */
	if (sel_start != -1) {
		hilight(vc, sel_start, sel_end);
		sel_start = -1;
	}
}

/*
 * Get character at selected position.
 */
static uint8_t sel_pos(struct vc *vc, int pos)
{
	uint16_t tc = vc->fb.buf[pos] & 0x00FF;
	int i;

	for (i = 0; i < 256; i++)
		if (vc->vc_translate[i] == tc)
			break;

	return i;
}

/*
 * Set selection.
 */
static int set_selection(unsigned long arg)
{
	int sel_mode, ps, pe, tmp, new_sel_start, new_sel_end, spc, i;
	struct vc *vc = &console_table[sel_cons];
	uint16_t *args = (uint16_t *) (arg + 1);
	uint16_t xs, ys, xe, ye;
	char *obp, *bp;

	/* console changed */
	if (sel_cons != fg_console) {
		clear_selection(vc);
		sel_cons = fg_console;
		vc = &console_table[sel_cons];
	}

	/* get arguments */
	xs = *(args++) - 1;
	ys = *(args++) - 1;
	xe = *(args++) - 1;
	ye = *(args++) - 1;
	sel_mode = *args;

	/* limit x/y to screen */
	if (xs > vc->fb.width - 1)
		xs = vc->fb.width - 1;
	if (xe > vc->fb.width - 1)
		xe = vc->fb.width - 1;
	if (ys > vc->fb.height - 1)
		ys = vc->fb.height - 1;
	if (ye > vc->fb.height - 1)
		ye = vc->fb.height - 1;
	ps = ys * vc->fb.width + xs;
	pe = ye * vc->fb.width + xe;

	/* switch start/end if needed */
	if (ps > pe) {
		tmp = ps;
		ps = pe;
		pe = tmp;
	}

	/* selection mode */
	switch (sel_mode) {
		case 0:							/* character by character selection */
			new_sel_start = ps;
			new_sel_end = pe;
			break;
		case 1:							/* word by word selection */
			spc = isspace(sel_pos(vc, ps));
			for (new_sel_start = ps; ; ps -= 1) {
				if ((spc && !isspace(sel_pos(vc, ps))) || (!spc && !inword(sel_pos(vc, ps))))
					break;
				new_sel_start = ps;
				if (!(ps % vc->fb.width))
					break;
			}

			spc = isspace(sel_pos(vc, pe));
			for (new_sel_end = pe; ; pe += 1) {
				if ((spc && !isspace(sel_pos(vc, pe))) || (!spc && !inword(sel_pos(vc, pe))))
					break;
				new_sel_end = pe;
				if (!((pe + 1) % vc->fb.width))
					break;
			}

			break;
		case 2:							/* line by line selection */
			new_sel_start = ps - ps % vc->fb.width;
			new_sel_end = pe + vc->fb.width - pe % vc->fb.width - 1;
			break;
		case 3:							/* update pointer */
			highlight_pointer(vc, pe);
			return 0;
		default:
			printf("Unknown sel_mode = %d\n", sel_mode);
			return 0;
	}

	/* remove old pointer */
	highlight_pointer(vc, -1);

	/* select to end of line if on trailing space */
	if (new_sel_end > new_sel_start && !atedge(vc, new_sel_end) && isspace(sel_pos(vc, new_sel_end))) {
		for (pe = new_sel_end + 1; ; pe += 1)
			if (!isspace(sel_pos(vc, pe)) || atedge(vc, pe))
				break;

		if (isspace(sel_pos(vc, pe)))
			new_sel_end = pe;
	}

	if (sel_start == -1) {							/* new selection */
		hilight(vc, new_sel_start, new_sel_end);
	} else if (new_sel_start == sel_start) {
		if (new_sel_end == sel_end)
			return 0;
		else if (new_sel_end > sel_end)					/* extend to right */
			hilight(vc, sel_end + 1, new_sel_end);
		else								/* contract from right */
			hilight(vc, new_sel_end + 1, sel_end);
	} else if (new_sel_end == sel_end) {
		if (new_sel_start < sel_start)					/* extend to left */
			hilight(vc, new_sel_start, sel_start - 1);
		else								/* contract from left*/
			hilight(vc, sel_start, new_sel_start - 1);
	} else {
		clear_selection(vc);
		hilight(vc, new_sel_start, new_sel_end);
	}

	/* set selection */
	sel_start = new_sel_start;
	sel_end = new_sel_end;

	/* free old buffer */
	if (sel_buf)
		kfree(sel_buf);

	/* allocate new buffer */
	sel_buf = (char *) kmalloc(sel_end - sel_start + 1);
	if (!sel_buf) {
		clear_selection(vc);
		return -ENOMEM;
	}

	/* copy selection to buffer */
	obp = bp = sel_buf;
	for (i = sel_start; i <= sel_end; i++) {
		*bp = sel_pos(vc, i);
		if (!isspace(*bp++))
			obp = bp;
		if (!((i + 1) % vc->fb.width)) {
			if (obp != bp) {
				bp = obp;
				*bp++ = '\r';
			}

			obp = bp;
		}
	}

	/* set buffer length */
	sel_buf_len = bp - sel_buf;

	return 0;
}

/*
 * Paste selection.
 */
int paste_selection(struct tty *tty)
{
	/* clear selection */
	clear_selection(tty->driver_data);

	/* no buffer */
	if (sel_buf_len <= 0)
		return 0;

	/* write buffer to tty */
	ring_buffer_write(&tty->read_queue, (uint8_t *) sel_buf, sel_buf_len);
	tty_do_cook(tty);

	return 0;
}

/*
 * Linux IOCTL.
 */
int tioclinux(struct tty *tty, unsigned long arg)
{
	switch (*((char *) arg)) {
		case 2:
			return set_selection(arg);
		case 3:
			return paste_selection(tty);
		case 5:
			return sel_loadlut(arg);
		case 6:
			*((char *) arg) = shift_state;
			return 0;
		default:
			printf("Unknown tioclinux ioctl: %d\n", *((char *) arg));
			break;
	}

	return -ENOIOCTLCMD;
}
