#ifndef _KEYBOARD_H_
#define _KEYBOARD_H_

#define KEYBOARD_PORT			0x60
#define KEYBOARD_ACK			0x61
#define KEYBOARD_STATUS			0x64
#define KEYBOARD_LED_CODE		0xED
#define KEYBOARD_RESET			0xFE

#define KEYBOARD_STATUS_SHIFT		0x01
#define KEYBOARD_STATUS_CTRL		0x02
#define KEYBOARD_STATUS_ALT		0x04
#define KEYBOARD_STATUS_CAPSLOCK	0x08

#define KEY_F1				0x3B
#define KEY_F10				0x44
#define KEY_HOME			0xE0
#define KEY_END				0xE1
#define KEY_UP				0xE2
#define KEY_DOWN			0xE3
#define KEY_LEFT			0xE4
#define KEY_RIGHT			0xE5
#define KEY_PAGEUP			0xE6
#define KEY_PAGEDOWN			0xE7
#define KEY_INSERT			0xE8
#define KEY_DELETE			0xE9

void init_keyboard();

#endif
