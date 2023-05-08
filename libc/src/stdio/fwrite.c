#include <stdio.h>

#include "__stdio_impl.h"

size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *fp)
{
	size_t k, len = size * nmemb;

	if (!size)
		nmemb = 0;

	k = __fwritex(ptr, len, fp);

	return k == len ? nmemb : k / size;
}
