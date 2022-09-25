#include <drivers/keyboard.h>
#include <x86/interrupt.h>
#include <x86/io.h>
#include <drivers/tty.h>
#include <ipc/signal.h>
#include <dev.h>

#define ARRAY_SIZE(x)		(sizeof(x) / sizeof((x)[0]))

/* shift state counters */
static uint8_t k_down[NR_SHIFT] = { 0, };

/* keyboard state */
static int shift_state = 0;
static int capslock_state = 0;
static int numlock_state = 0;
static uint8_t prev_scan_code = 0;

typedef void (*k_hand)(struct tty_t *tty, uint8_t value, char up_flag);
typedef void (k_handfn)(struct tty_t *tty, uint8_t value, char up_flag);
typedef void (*void_fnp)(struct tty_t *tty);
typedef void (void_fn)(struct tty_t *tty);

/*
 * Key handlers functions.
 */
static k_handfn
	do_self,	do_fn,		do_spec,	do_pad,
	do_dead,	do_cons,	do_cur,		do_shift,
	do_meta,	do_ascii,	do_lock,	do_lowercase,
	do_slock,	do_ignore;

/*
 * Key handlers.
 */
static k_hand key_handler[] = {
	do_self,	do_fn,		do_spec,	do_pad,
	do_dead,	do_cons,	do_cur,		do_shift,
	do_meta,	do_ascii,	do_lock,	do_lowercase,
	do_slock,	do_ignore,	do_ignore,	do_ignore
};

/*
 * Key functions handlers functions.
 */
static void_fn
	fn_null,		fn_enter,		fn_show_ptregs,		fn_show_mem,
	fn_show_state,		fn_send_intr,		fn_lastcons,		fn_caps_toggle,
	fn_num,			fn_hold,		fn_scroll_forw,		fn_scroll_back,
	fn_boot_it,		fn_caps_on,		fn_compose,		fn_SAK,
	fn_decr_console,	fn_incr_console,	fn_spawn_console, 	fn_bare_num;

/*
 * Key functions handlers.
 */
static void_fnp spec_fn_table[] = {
	fn_null,	fn_enter,		fn_show_ptregs,		fn_show_mem,		fn_show_state,
	fn_send_intr,	fn_lastcons,		fn_caps_toggle,		fn_num,			fn_hold,
	fn_scroll_forw,	fn_scroll_back, 	fn_boot_it,		fn_caps_on,		fn_compose,
	fn_SAK, 	fn_decr_console,	fn_incr_console,	fn_spawn_console, 	fn_bare_num
};

/* maximum values each key_handler can handle */
const int max_vals[] = {
	255, ARRAY_SIZE(func_table) - 1, ARRAY_SIZE(spec_fn_table) - 1, NR_PAD - 1,
	NR_DEAD - 1, 255, 3, NR_SHIFT - 1, 255, NR_ASCII - 1, NR_LOCK - 1,
	255, NR_LOCK - 1, 255, NR_BRL - 1
};

const int NR_TYPES = ARRAY_SIZE(max_vals);

/*
 * Put a character in tty read queue.
 */
static void putc(struct tty_t *tty, uint8_t c)
{
	if (!ring_buffer_full(&tty->read_queue))
		ring_buffer_write(&tty->read_queue, &c, 1);
}

/*
 * Put a string in tty read queue.
 */
static void puts(struct tty_t *tty, uint8_t *s)
{
	while (*s) {
		putc(tty, *s);
		s++;
	}
}

/*
 * Apply a key.
 */
static void applkey(struct tty_t *tty, int key, char mode)
{
	static uint8_t buf[] = { 0x1b, 'O', 0x00, 0x00 };

	buf[1] = (mode ? 'O' : '[');
	buf[2] = key;
	puts(tty, buf);
}

/*
 * Null function.
 */
static void fn_null(struct tty_t *tty)
{
	UNUSED(tty);
}

/*
 * Enter function.
 */
static void fn_enter(struct tty_t *tty)
{
	putc(tty, 13);
}

/*
 * Show registers function.
 */
static void fn_show_ptregs(struct tty_t *tty)
{
	UNUSED(tty);
}

/*
 * Show memory function.
 */
static void fn_show_mem(struct tty_t *tty)
{
	UNUSED(tty);
}

/*
 * Show state function.
 */
static void fn_show_state(struct tty_t *tty)
{
	UNUSED(tty);
}

/*
 * Send interrupt function.
 */
static void fn_send_intr(struct tty_t *tty)
{
	UNUSED(tty);
}

/*
 * Last console function.
 */
static void fn_lastcons(struct tty_t *tty)
{
	UNUSED(tty);
}

/*
 * Caps toggle function.
 */
static void fn_caps_toggle(struct tty_t *tty)
{
	UNUSED(tty);
	capslock_state ^= 1;
}

/*
 * Num function.
 */
static void fn_num(struct tty_t *tty)
{
	UNUSED(tty);
	numlock_state ^= 1;
}

/*
 * Hold function.
 */
static void fn_hold(struct tty_t *tty)
{
	UNUSED(tty);
}

/*
 * Scroll forward function.
 */
static void fn_scroll_forw(struct tty_t *tty)
{
	UNUSED(tty);
}

/*
 * Scroll back function.
 */
static void fn_scroll_back(struct tty_t *tty)
{
	UNUSED(tty);
}

/*
 * Boot it function.
 */
static void fn_boot_it(struct tty_t *tty)
{
	UNUSED(tty);
}

/*
 * Caps on function.
 */
static void fn_caps_on(struct tty_t *tty)
{
	UNUSED(tty);
}

/*
 * Compose function.
 */
static void fn_compose(struct tty_t *tty)
{
	UNUSED(tty);
}

/*
 * SAK function.
 */
static void fn_SAK(struct tty_t *tty)
{
	UNUSED(tty);
}

/*
 * Decrement console function.
 */
static void fn_decr_console(struct tty_t *tty)
{
	UNUSED(tty);
}

/*
 * Increment console function.
 */
static void fn_incr_console(struct tty_t *tty)
{
	UNUSED(tty);
}

/*
 * Spawn console function.
 */
static void fn_spawn_console(struct tty_t *tty)
{
	UNUSED(tty);
}

/*
 * Bare num function.
 */
static void fn_bare_num(struct tty_t *tty)
{
	UNUSED(tty);
	numlock_state ^= 1;
}

/*
 * Handle normal key.
 */
static void do_self(struct tty_t *tty, uint8_t value, char up_flag)
{
	if (up_flag)
		return;

	putc(tty, value);
}

/*
 * Handle function keys.
 */
static void do_fn(struct tty_t *tty, uint8_t value, char up_flag)
{
	if (up_flag)
		return;

	/* get function */
	if (func_table[value])
		puts(tty, func_table[value]);
}

/*
 * Handle special key.
 */
static void do_spec(struct tty_t *tty, uint8_t value, char up_flag)
{
	if (up_flag)
		return;

	if (value >= ARRAY_SIZE(spec_fn_table))
		return;

	spec_fn_table[value](tty);
}

/*
 * Handle pad key.
 */
static void do_pad(struct tty_t *tty, uint8_t value, char up_flag)
{
	static const char *pad_chars = "0123456789+-*/\015,.?";

	if (up_flag)
		return;

	/* when numlock is down, handle function keys */
	if (!numlock_state) {
		switch (value) {
			case KVAL(K_PCOMMA):
			case KVAL(K_PDOT):
				do_fn(tty, KVAL(K_REMOVE), 0);
				return;
			case KVAL(K_P0):
				do_fn(tty, KVAL(K_INSERT), 0);
				return;
			case KVAL(K_P1):
				do_fn(tty, KVAL(K_SELECT), 0);
				return;
			case KVAL(K_P2):
				do_cur(tty, KVAL(K_DOWN), 0);
				return;
			case KVAL(K_P3):
				do_fn(tty, KVAL(K_PGDN), 0);
				return;
			case KVAL(K_P4):
				do_cur(tty, KVAL(K_LEFT), 0);
				return;
			case KVAL(K_P6):
				do_cur(tty, KVAL(K_RIGHT), 0);
				return;
			case KVAL(K_P7):
				do_fn(tty, KVAL(K_FIND), 0);
				return;
			case KVAL(K_P8):
				do_cur(tty, KVAL(K_UP), 0);
				return;
			case KVAL(K_P9):
				do_fn(tty, KVAL(K_PGUP), 0);
				return;
		}
	}

	/* just print pad character */
	putc(tty, pad_chars[value]);
}

/*
 * Handle dead key.
 */
static void do_dead(struct tty_t *tty, uint8_t value, char up_flag)
{
	UNUSED(tty);
	UNUSED(value);
	UNUSED(up_flag);
}

/*
 * Handle cons key.
 */
static void do_cons(struct tty_t *tty, uint8_t value, char up_flag)
{
	UNUSED(tty);
	UNUSED(value);
	UNUSED(up_flag);
}

/*
 * Handle cursor key.
 */
static void do_cur(struct tty_t *tty, uint8_t value, char up_flag)
{
	static const char *cur_chars = "BDCA";

	if (up_flag)
		return;

	applkey(tty, cur_chars[value], 0);
}

/*
 * Handle shift key.
 */
static void do_shift(struct tty_t *tty, uint8_t value, char up_flag)
{
	/* unused tty */
	UNUSED(tty);

	/* update shift counters */
	if (up_flag) {
		if (k_down[value])
			k_down[value]--;
	} else {
		k_down[value]++;
	}

	/* update shift state */
	if (k_down[value])
		shift_state |= (1 << value);
	else
		shift_state &= ~(1 << value);
}

/*
 * Handle meta key.
 */
static void do_meta(struct tty_t *tty, uint8_t value, char up_flag)
{
	UNUSED(tty);
	UNUSED(value);
	UNUSED(up_flag);
}

/*
 * Handle ascii key.
 */
static void do_ascii(struct tty_t *tty, uint8_t value, char up_flag)
{
	UNUSED(tty);
	UNUSED(value);
	UNUSED(up_flag);
}

/*
 * Handle lock key.
 */
static void do_lock(struct tty_t *tty, uint8_t value, char up_flag)
{
	UNUSED(tty);
	UNUSED(value);
	UNUSED(up_flag);
}

/*
 * Handle lower case key.
 */
static void do_lowercase(struct tty_t *tty, uint8_t value, char up_flag)
{
	UNUSED(tty);
	UNUSED(value);
	UNUSED(up_flag);
}

/*
 * Handle slock key.
 */
static void do_slock(struct tty_t *tty, uint8_t value, char up_flag)
{
	UNUSED(tty);
	UNUSED(value);
	UNUSED(up_flag);
}

/*
 * Ignore key.
 */
static void do_ignore(struct tty_t *tty, uint8_t value, char up_flag)
{
	UNUSED(tty);
	UNUSED(value);
	UNUSED(up_flag);
}

/*
 * Scan keyboard key.
 */
static uint8_t scan_key()
{
	uint8_t code, value;

	/* get code and value */
	code = inb(KEYBOARD_PORT);
	value = inb(KEYBOARD_ACK);

	/* ack keyboard value */
	outb(KEYBOARD_ACK, value | 0x80);
	outb(KEYBOARD_ACK, value);

	return code;
}

/*
 * Keyboard handler.
 */
static void keyboard_handler(struct registers_t *regs)
{
	uint8_t scan_code, key_code = 0, type, shift_final;
	uint16_t *key_map, key_sym;
	struct tty_t *tty;
	char up_flag;

	UNUSED(regs);

	/* get current tty */
	tty = tty_lookup(DEV_TTY0);
	if (!tty)
		return;

	/* get scan code */
	scan_code = scan_key();
	if (scan_code == 0xE0 || scan_code == 0xE1) {
		prev_scan_code = scan_code;
		return;
	}

	/* convert scan code to key code */
	up_flag = scan_code & 0200;
	scan_code &= 0x7F;

	/* handle e0/e1 key */
	if (prev_scan_code) {
		if (prev_scan_code != 0xE0) {
			/* pause key */
			if (prev_scan_code == 0xE1 && scan_code == 0x1D) {
				prev_scan_code = (uint8_t) 0x100;
				return;
			} else if (prev_scan_code == (uint8_t) 0x100 && scan_code == 0x45) {
				key_code = E1_PAUSE;
				prev_scan_code = 0;
			} else {
				prev_scan_code = 0;
				return;
			}
		} else {
			prev_scan_code = 0;

			if (scan_code == 0x2A || scan_code == 0x36)
				return;

			if (e0_keys[scan_code])
				key_code = e0_keys[scan_code];
			else
				return;
		}
	} else {
		key_code = scan_code;
	}

	/* get key map */
	shift_final = shift_state ^ capslock_state;
	key_map = key_maps[shift_final];

	/* handle key */
	if (key_map) {
		/* get key sym/type */
		key_sym = key_map[key_code];
		type = KTYP(key_sym);

		if (type >= 0xF0) {
			type -= 0xF0;
			if (type == KT_LETTER) {
				type = KT_LATIN;
			}

			/* handle key */
			(*key_handler[type])(tty, key_sym & 0xFF, up_flag);
		}
	}

	/* do cook */
	tty_do_cook(tty);
}

/*
 * Init keyboard.
 */
void init_keyboard()
{
	register_interrupt_handler(33, keyboard_handler);
}
