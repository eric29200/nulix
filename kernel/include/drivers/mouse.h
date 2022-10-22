#ifndef _MOUSE_H_
#define _MOUSE_H_

#include <stddef.h>

struct mouse_event_t {
	int32_t			x;
	int32_t			y;
	uint8_t			buttons;
	uint32_t		state;
};

void init_mouse();

extern struct inode_operations_t mouse_iops;

#endif
