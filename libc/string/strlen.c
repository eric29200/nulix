#include <string.h>

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
