#include <string.h>

char *strncat(char *dest, const char *src, size_t n)
{
	char *ret = dest;

	for (dest += strlen(dest); n; n--)
		*dest++ = *src++;
	*dest++ = 0;

	return ret;
}
