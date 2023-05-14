#include <string.h>

size_t strspn(const char *s, const char *accept)
{
	size_t len = 0;

	for (; *s; s++) {
		if (strchr(accept, *s) == NULL)
			break;
		len++;
	}

	return len;
}