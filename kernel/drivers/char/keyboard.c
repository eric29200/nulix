#include <drivers/char/keyboard.h>
#include <drivers/char/tty.h>
#include <x86/interrupt.h>
#include <x86/io.h>
#include <ipc/signal.h>

#define ARRAY_SIZE(x)		(sizeof(x) / sizeof((x)[0]))

/* shift state counters */
static uint8_t k_down[NR_SHIFT] = { 0, };

/* keyboards table */
struct kbd kbd_table[NR_CONSOLES];
static struct tty *tty = NULL;

/* keyboard state */
int shift_state = 0;
static int capslock_state = 0;
static int numlock_state = 0;
static uint8_t prev_scan_code = 0;

typedef void (*k_hand)(uint8_t value, char up_flag);
typedef void (k_handfn)(uint8_t value, char up_flag);
typedef void (*void_fnp)();
typedef void (void_fn)();

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
static void putc(uint8_t c)
{
	tty_queue_putc(&tty->read_queue, c);
}

/*
 * Put a string in tty read queue.
 */
static void puts(uint8_t *s)
{
	while (*s) {
		putc(*s);
		s++;
	}
}

/*
 * Apply a key.
 */
static void applkey(int key, char mode)
{
	static uint8_t buf[] = { 0x1b, 'O', 0x00, 0x00 };

	buf[1] = (mode ? 'O' : '[');
	buf[2] = key;
	puts(buf);
}

/*
 * Null function.
 */
static void fn_null()
{
}

/*
 * Enter function.
 */
static void fn_enter()
{
	putc(13);
}

/*
 * Show registers function.
 */
static void fn_show_ptregs()
{
}

/*
 * Show memory function.
 */
static void fn_show_mem()
{
}

/*
 * Show state function.
 */
static void fn_show_state()
{
}

/*
 * Send interrupt function.
 */
static void fn_send_intr()
{
}

/*
 * Last console function.
 */
static void fn_lastcons()
{
}

/*
 * Caps toggle function.
 */
static void fn_caps_toggle()
{
	capslock_state ^= 1;
}

/*
 * Num function.
 */
static void fn_num()
{
	numlock_state ^= 1;
}

/*
 * Hold function.
 */
static void fn_hold()
{
}

/*
 * Scroll forward function.
 */
static void fn_scroll_forw()
{
}

/*
 * Scroll back function.
 */
static void fn_scroll_back()
{
}

/*
 * Boot it function.
 */
static void fn_boot_it()
{
}

/*
 * Caps on function.
 */
static void fn_caps_on()
{
}

/*
 * Compose function.
 */
static void fn_compose()
{
}

/*
 * SAK function.
 */
static void fn_SAK()
{
}

/*
 * Decrement console function.
 */
static void fn_decr_console()
{
}

/*
 * Increment console function.
 */
static void fn_incr_console()
{
}

pid_t spawnpid;
int spawnsig;

/*
 * Spawn console function.
 */
static void fn_spawn_console()
{
	if (spawnpid && kill_proc(spawnpid, spawnsig, 1))
		spawnpid = 0;
}

/*
 * Bare num function.
 */
static void fn_bare_num()
{
	numlock_state ^= 1;
}

/*
 * Handle normal key.
 */
static void do_self(uint8_t value, char up_flag)
{
	if (up_flag)
		return;

	putc(value);
}

/*
 * Handle function keys.
 */
static void do_fn(uint8_t value, char up_flag)
{
	if (up_flag)
		return;

	/* get function */
	if (func_table[value])
		puts(func_table[value]);
}

/*
 * Handle special key.
 */
static void do_spec(uint8_t value, char up_flag)
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
static void do_pad(uint8_t value, char up_flag)
{
	static const char *pad_chars = "0123456789+-*/\015,.?";

	if (up_flag)
		return;

	/* when numlock is down, handle function keys */
	if (!numlock_state) {
		switch (value) {
			case KVAL(K_PCOMMA):
			case KVAL(K_PDOT):
				do_fn(KVAL(K_REMOVE), 0);
				return;
			case KVAL(K_P0):
				do_fn(KVAL(K_INSERT), 0);
				return;
			case KVAL(K_P1):
				do_fn(KVAL(K_SELECT), 0);
				return;
			case KVAL(K_P2):
				do_cur(KVAL(K_DOWN), 0);
				return;
			case KVAL(K_P3):
				do_fn(KVAL(K_PGDN), 0);
				return;
			case KVAL(K_P4):
				do_cur(KVAL(K_LEFT), 0);
				return;
			case KVAL(K_P6):
				do_cur(KVAL(K_RIGHT), 0);
				return;
			case KVAL(K_P7):
				do_fn(KVAL(K_FIND), 0);
				return;
			case KVAL(K_P8):
				do_cur(KVAL(K_UP), 0);
				return;
			case KVAL(K_P9):
				do_fn(KVAL(K_PGUP), 0);
				return;
		}
	}

	/* just print pad character */
	putc(pad_chars[value]);
}

/*
 * Handle dead key.
 */
static void do_dead(uint8_t value, char up_flag)
{
	UNUSED(value);
	UNUSED(up_flag);
}

/*
 * Handle console key.
 */
static void do_cons(uint8_t value, char up_flag)
{
	if (up_flag)
		return;

	/* change console */
	console_change(value);
}

/*
 * Handle cursor key.
 */
static void do_cur(uint8_t value, char up_flag)
{
	static const char *cur_chars = "BDCA";

	if (up_flag)
		return;

	applkey(cur_chars[value], 0);
}

/*
 * Handle shift key.
 */
static void do_shift(uint8_t value, char up_flag)
{
	if (value == KVAL(K_CAPSSHIFT))
		value = KVAL(K_SHIFT);

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
static void do_meta(uint8_t value, char up_flag)
{
	UNUSED(value);
	UNUSED(up_flag);
}

/*
 * Handle ascii key.
 */
static void do_ascii(uint8_t value, char up_flag)
{
	UNUSED(value);
	UNUSED(up_flag);
}

/*
 * Handle lock key.
 */
static void do_lock(uint8_t value, char up_flag)
{
	UNUSED(value);
	UNUSED(up_flag);
}

/*
 * Handle lower case key.
 */
static void do_lowercase(uint8_t value, char up_flag)
{
	UNUSED(value);
	UNUSED(up_flag);
}

/*
 * Handle slock key.
 */
static void do_slock(uint8_t value, char up_flag)
{
	UNUSED(value);
	UNUSED(up_flag);
}

/*
 * Ignore key.
 */
static void do_ignore(uint8_t value, char up_flag)
{
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
static void keyboard_handler(struct registers *regs)
{
	uint8_t scan_code, key_code = 0, type, shift_final;
	uint16_t *key_map, key_sym;
	struct kbd *kbd;
	char up_flag;

	/* unused registers */
	UNUSED(regs);

	/* get current console and keyboard */
	tty = &tty_table[fg_console];
	kbd = &kbd_table[fg_console];

	/* get scan code */
	scan_code = scan_key();
	if (kbd->kbd_mode == VC_RAW)
		putc(scan_code);

	if (scan_code == 0xE0 || scan_code == 0xE1) {
		prev_scan_code = scan_code;
		goto end;
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
				goto end;
			} else if (prev_scan_code == (uint8_t) 0x100 && scan_code == 0x45) {
				key_code = E1_PAUSE;
				prev_scan_code = 0;
			} else {
				prev_scan_code = 0;
				goto end;
			}
		} else {
			prev_scan_code = 0;

			if (scan_code == 0x2A || scan_code == 0x36)
				goto end;

			if (e0_keys[scan_code])
				key_code = e0_keys[scan_code];
			else
				goto end;
		}
	} else {
		key_code = scan_code;
	}

	/* raw mode */
	if (kbd->kbd_mode == VC_RAW) {
		goto end;
	} else if (kbd->kbd_mode == VC_MEDIUMRAW) {
		putc(key_code + up_flag);
		goto end;
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
			if (type == KT_LETTER)
				type = KT_LATIN;

			/* handle key */
			(*key_handler[type])(key_sym & 0xFF, up_flag);
		}
	}

end:
	/* do cook */
	tty_do_cook(tty);
}

/*
 * Init keyboard.
 */
void init_keyboard()
{
	struct kbd kbd0;
	int i;

 	/* set keyboard */
	memset(&kbd0, 0, sizeof(struct kbd));
	kbd0.kbd_default_led_flag_state = KBD_DEFLEDS;
	kbd0.kbd_led_flag_state = kbd0.kbd_default_led_flag_state;
	kbd0.kbd_led_mode = LED_SHOW_FLAGS;

	/* set keyboards table */
	for (i = 0; i < NR_CONSOLES; i++)
		kbd_table[i] = kbd0;

	/* register interrupt handler */
	request_irq(1, keyboard_handler, "keyboard");
}
