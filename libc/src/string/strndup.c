#include <string.h>
#include <stdio.h>
#include <stdlib.h>

char *strndup(const char *s, size_t n)
{
	size_t len;
	char *new;

	len = strnlen(s, n);
	new = malloc(len + 1);
	if (!new)
		return NULL;

	new[len] = '\0';
	return (char *) memcpy(new, s, len);
}