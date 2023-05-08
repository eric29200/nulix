#include <stdio.h>

#include "__stdio_impl.h"

int ferror(FILE *fp)
{
	return !!(fp->flags & F_ERR);
}