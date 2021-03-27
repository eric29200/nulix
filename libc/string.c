#include <libc/string.h>
#include <libc/stddef.h>

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

char *strchr(const char *s, char c);

/*
 * Fill memory with a constant byte.
 */
void memset(void *s, char c, size_t n)
{
  char *sp = (char *) s;

  for (; n != 0; n--)
    *sp++ = c;
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