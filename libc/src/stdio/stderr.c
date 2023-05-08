#include <stdio.h>

#include "__stdio_impl.h"

static unsigned char buf[UNGET];

FILE __stderr_FILE = {
	.buf 		= buf + UNGET,
	.buf_size	= 0,
	.fd		= 2,
	.flags 		= F_PERM | F_NORD,
	.lbf		= -1,
	.write 		= __stdio_write,
	.seek 		= __stdio_seek,
};

FILE *stderr = & __stderr_FILE;