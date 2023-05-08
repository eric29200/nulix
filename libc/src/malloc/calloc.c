#include <stdlib.h>
#include <string.h>

void *calloc(size_t nmemb, size_t size)
{
	void *block;

	block = malloc(nmemb * size);
	if (block)
		memset(block, 0, nmemb * size);

	return block;
}