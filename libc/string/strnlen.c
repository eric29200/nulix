#include <string.h>

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
