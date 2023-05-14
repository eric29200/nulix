#include <stdio.h>
#include <string.h>

char *strchrnul(const char *str, int c)
{
	for (; *str; str++)
		if (*str == c)
			break;

	return (char *) str;
}