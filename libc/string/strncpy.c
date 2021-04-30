#include <string.h>

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
