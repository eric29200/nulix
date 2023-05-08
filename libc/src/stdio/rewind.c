
#include <stdio.h>
#include <fcntl.h>

#include "__stdio_impl.h"

void rewind(FILE *fp)
{
	fseek(fp, 0, SEEK_SET);
	fp->flags &= ~F_ERR;
}