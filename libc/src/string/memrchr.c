#include <stdio.h>
#include <string.h>

void *memrchr(const void *s, int c, size_t n)
{
	const unsigned char *str = s;
	c = (unsigned char) c;

	while (n--)
		if (str[n] == c)
			return (void *) (str + n);

	return NULL;
}