#ifndef _LIBC_LINUX_KD_H_
#define _LIBC_LINUX_KD_H_

#include <stdint.h>

struct kbentry {
	uint8_t		kb_table;
	uint8_t		kb_index;
	uint16_t	kb_value;
};

#define KDGKBENT	0x4B46
#define KDSKBENT	0x4B47

#endif