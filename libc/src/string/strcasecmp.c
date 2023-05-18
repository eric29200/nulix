#include <string.h>
#include <ctype.h>

int strcasecmp(const char *s1, const char *s2)
{
	while (*s1 && (*s1 == *s2 || tolower(*s1) == tolower(*s2))) {
		s1++;
		s2++;
	}

	return tolower(*s1) - tolower(*s2);
}