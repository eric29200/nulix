#include <string.h>

char *strtok_r(char *str, const char *delim, char **p)
{
	if (!str && !(str = *p))
		return NULL;

	str += strspn(str, delim);
	if (!*str)
		return *p = NULL;

	*p = str + strcspn(str, delim);
	if (**p)
		*(*p)++ = 0;
	else
		*p = NULL;

	return str;
}