#ifndef _KD_H_
#define _KD_H_

#include <stddef.h>

#define K_RAW			0x00
#define K_XLATE			0x01
#define K_MEDIUMRAW		0x02
#define K_UNICODE		0x03
#define K_OFF			0x04

#define KB_84			0x01
#define KB_101			0x02
#define KB_OTHER		0x03

#define KD_TEXT			0x00
#define KD_GRAPHICS		0x01

#define KDGKBTYPE		0x4B33
#define KDSETMODE		0x4B3A
#define KDGETMODE		0x4B3B
#define KDGKBMODE		0x4B44
#define KDSKBMODE		0x4B45
#define KDGKBENT		0x4B46
#define KDSKBENT		0x4B47
#define KDGKBSENT		0x4B48
#define KDSKBSENT		0x4B49
#define	KDGKBLED		0x4B64
#define KDSKBLED		0x4B65

#define U(x)			((x) ^ 0xF000)

/*
 * Keyboard entry.
 */
struct kbentry_t {
	uint8_t		kb_table;
	uint8_t		kb_index;
	uint16_t	kb_value;
};

/*
 * Keyboard function entry.
 */
struct kbsentry_t {
	uint8_t		kb_func;
	uint8_t		kb_string[256];
};

#endif
