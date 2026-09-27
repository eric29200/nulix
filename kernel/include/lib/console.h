#ifndef _LIB_CONSOLE_H_
#define _LIB_CONSOLE_H_

#include <lib/list.h>
#include <stddef.h>

#define CON_ENABLED		4

/*
 * Console structure.
 */
struct console {
	char			name[8];
	int			index;
	uint32_t		flags;
	void			(*write)(struct console *, const char *, size_t);
	int			(*setup)(struct console *);
	struct list_head	list;
};

void register_console(struct console *console);

#endif