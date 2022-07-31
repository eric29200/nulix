#include <mm/mm.h>
#include <string.h>
#include <stddef.h>

/*
 * Compute length of a string
 */
size_t strlen(const char *s)
{
	size_t size = 0;

	while (s[size] != '\0')
		size++;

	return size;
}

/*
 * Compute length of a string
 */
size_t strnlen(const char *s, size_t maxlen)
{
	size_t size = 0;

	while (s[size] != '\0' && maxlen-- > 0)
		size++;

	return size;
}

/*
 * Compare two strings.
 */
int strcmp(const char *s1, const char *s2)
{
	while (*s1 && *s1 == *s2) {
		s1++;
		s2++;
	}

	return *s1 - *s2;
}

/*
 * Compare two strings.
 */
int strncmp(const char *s1, const char *s2, size_t n)
{
	while (n > 0 && *s1 && *s1 == *s2) {
		n--;
		s1++;
		s2++;
	}

	if (n == 0)
		return 0;

	return *s1 - *s2;
}

/*
 * Copy a string.
 */
char *strcpy(char *dest, const char *src)
{
	return memcpy(dest, src, strlen(src) + 1);
}

/*
 * Copy a string.
 */
char *strncpy(char *dest, const char *src, size_t n)
{
	size_t size = strnlen(src, n);

	if (size != n)
		memset(dest + size, '\0', n - size);

	return memcpy(dest, src, size);
}

/*
 * Duplicate a string.
 */
char *strdup(const char *s)
{
	size_t len;
	void *new;

	len = strlen(s) + 1;
	new = kmalloc(len);
	if (!new)
		return NULL;

	return (char *) memcpy(new, s, len);
}

/*
 * Locate a character in a string.
 */
char *strchr(const char *s, char c)
{
	for (; *s; s++)
		if (*s == c)
			return (char *) s;

	return NULL;
}

/*
 * Get the length of the initial segment of s which consists entirely of bytes in accept.
 */
size_t strspn(const char *s, const char *accept)
{
	size_t l = 0;

	for (; *s; s++) {
		if (strchr(accept, *s) == NULL)
			break;
		l++;
	}

	return l;
}

/*
 * Get the length of the initial segment of s which consists entirely of bytes not in reject.
 */
size_t strcspn(const char *s, const char *reject)
{
	size_t l = 0;

	for (; *s; s++) {
		if (strchr(reject, *s) != NULL)
			break;
		l++;
	}

	return l;
}

/*
 * Fill memory with a constant byte.
 */
void memset(void *s, char v, size_t n)
{
	char *sp = (char *) s;

	for (; n != 0; n--)
		*sp++ = v;
}

/*
 * Fill memory with a constant word.
 */
void memsetw(void *s, short v, size_t n)
{
	short *sp = (short *) s;

	for (; n != 0; n--)
		*sp++ = v;
}

/*
 * Compare memory areas.
 */
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

/*
 * Copy memory area.
 */
void *memcpy(void *dest, const void *src, size_t n)
{
	const char *sp = src;
	char *dp = dest;

	while (n-- > 0)
		*dp++ = *sp++;

	return dest;
}

/*
 * Convert a string to an integer.
 */
int atoi(const char *s)
{
	unsigned int ret = 0;
	unsigned int d;
	int neg = 0;

	if (*s == '-') {
		neg = 1;
		s++;
	}

	for (;;) {
		d = (*s++) - '0';
		if (d > 9)
			break;
		ret *= 10;
		ret += d;
	}

	return neg ? -ret : ret;
}
