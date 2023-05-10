#include <stdlib.h>
#include <ctype.h>

int atoi(const char *s)
{
	int n = 0, neg = 0;

	while (isspace(*s))
		s++;

	if (*s == '-') {
		neg = 1;
		s++;
	} else if (*s == '+') {
		s++;
	}

	while (isdigit(*s))
		n = 10 * n - (*s++ - '0');

	return neg ? n : -n;
}