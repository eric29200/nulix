#include <string.h>

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
