#include <string.h>

/*
 * Split a string into tokens.
 */
char *strtok(char *s, const char *delim)
{
  static char *last;
  return strtok_r(s, delim, &last);
}
