#include <stdlib.h>
#include <unistd.h>

#include "../stdio/__stdio_impl.h"

void exit(int status)
{
	/* clear stdio */
	__stdio_exit();

	/* exit */
	_exit(status);
}