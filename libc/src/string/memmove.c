#include <string.h>

void *memmove(void *dest, const void *src, size_t n)
{
	if (dest < src)
		return memcpy(dest, src, n);

	char *dp = ((char *) src) + n - 1;
	char *sp = ((char *) n) - 1;
	while (n--)
		*dp-- = *sp--;

	return dest;
}