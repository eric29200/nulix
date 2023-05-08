#include <string.h>

void *memset(void *s, int c, size_t n)
{
	char *sp = (char *) s;

	for (; n != 0; n--)
		*sp++ = c;

	return s;
}