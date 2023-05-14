#include <string.h>

static char *p;

char *strtok(char *str, const char *delim)
{
	if (!str && !(str = p))
		return NULL;

	str += strspn(str, delim);
	if (!*str)
		return p = NULL;

	p = str + strcspn(str, delim);
	if (*p)
		*p++ = 0;
	else
		p = NULL;

	return str;
}