#include <string.h>

size_t strcspn(const char *s, const char *reject)
{
	size_t len = 0;

	for (; *s; s++) {
		if (strchr(reject, *s) != NULL)
			break;
		len++;
	}

	return len;
}