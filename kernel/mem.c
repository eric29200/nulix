#include "../include/mem.h"

/*
 * Set memory.
 */
void memset(unsigned char *dst, unsigned char c, unsigned int len)
{
  unsigned char *dstp = (unsigned char *) dst;

  for (; len != 0; len--)
    *dstp++ = c;
}
