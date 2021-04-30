#include <string.h>

/*
 * Copy a string.
 */
char *strcpy(char *dest, const char *src)
{
  return memcpy(dest, src, strlen(src) + 1);
}
