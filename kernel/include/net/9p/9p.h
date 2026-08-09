#ifndef _NET_9P_H_
#define _NET_9P_H_

#include <stdio.h>

/*
 * Print an error message.
 */
static inline void p9_error(const char *fmt, ...)
{
	va_list ap;

	/* print panic */
	printf("[9p ERROR] ");

	/* print in tmp buf */
	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
}

#endif