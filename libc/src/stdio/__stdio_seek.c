#include <unistd.h>

#include "__stdio_impl.h"

off_t __stdio_seek(FILE *fp, off_t off, int whence)
{
	return lseek(fp->fd, off, whence);
}