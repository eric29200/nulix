#include <string.h>

void *memcpy(void *dest, const void *src, size_t n)
{
	const char *sp = src;
	char *dp = dest;

	while (n-- > 0)
		*dp++ = *sp++;

	return dest;
}