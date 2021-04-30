#include <string.h>

/*
 * Fill memory with a constant byte.
 */
void memset(void *s, char c, size_t n)
{
  char *sp = (char *) s;

  for (; n != 0; n--)
    *sp++ = c;
}
