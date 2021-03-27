#include <libc/string.h>
#include <libc/stddef.h>

/*
 * Set memory.
 */
void memset(void *s, char c, size_t n)
{
  uint8_t *sp = (uint8_t *) s;

  for (; n != 0; n--)
    *sp++ = c;
}
