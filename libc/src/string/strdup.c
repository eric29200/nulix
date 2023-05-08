#include <string.h>
#include <stdio.h>
#include <stdlib.h>

char *strdup(const char *s)
{
	size_t len;
	char *new;

	len = strlen(s);
	new = malloc(len + 1);
	if (!new)
		return NULL;

	new[len] = '\0';
	return (char *) memcpy(new, s, len);
}