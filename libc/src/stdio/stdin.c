#include <stdio.h>

#include "__stdio_impl.h"

static unsigned char buf[BUFSIZ + UNGET];

FILE __stdin_FILE = {
	.buf 		= buf + UNGET,
	.buf_size	= BUFSIZ,
	.fd		= 0,
	.flags 		= F_PERM | F_NOWR,
	.read 		= __stdio_read,
	.seek 		= __stdio_seek,
};

FILE *stdin = & __stdin_FILE;