#include <stdio.h>

#include "__stdio_impl.h"

int feof(FILE *fp)
{
	return !!(fp->flags & F_EOF);
}