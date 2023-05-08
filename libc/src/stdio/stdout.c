#include <stdio.h>

#include "__stdio_impl.h"

static unsigned char buf[BUFSIZ + UNGET];

FILE __stdout_FILE = {
	.buf 		= buf + UNGET,
	.buf_size	= BUFSIZ,
	.fd		= 1,
	.flags 		= F_PERM | F_NORD,
	.lbf		= '\n',
	.write 		= __stdio_write,
	.seek 		= __stdio_seek,
};

FILE *stdout = & __stdout_FILE;