#include <string.h>

int memcmp(const void *s1, const void *s2, size_t n)
{
	const char *sp1 = (const char *) s1;
	const char *sp2 = (const char *) s2;

	while (n-- > 0) {
		if (*sp1 != *sp2)
			return (int) *sp1 - (int) *sp2;

		sp1++;
		sp2++;
	}

	return 0;
}