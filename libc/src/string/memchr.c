#include <stdio.h>
#include <string.h>

void *memchr(const void *s, int c, size_t n)
{
	const unsigned char *str = s;

	for (; n > 0; str++, n--)
		if (*str == c)
			return (void *) str;

	return NULL;
}