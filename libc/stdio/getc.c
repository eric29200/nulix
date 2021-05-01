#include <stdio.h>
#include <unistd.h>

/*
 * Read a character from stdin.
 */
int getc()
{
  int c;
  read(STDIN, &c, 1);
  return c;
}
