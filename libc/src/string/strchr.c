#include <stdio.h>
#include <string.h>

char *strchr(const char *str, int c)
{
	for (; *str; str++)
		if (*str == c)
			return (char *) str;

	return NULL;
}