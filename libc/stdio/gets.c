#include <stdio.h>

/*
 * Read a line from stdin.
 */
char *gets(char *s, int size)
{
  int i;

  /* read all characters */
  for (i = 0; i < size - 1; i++) {
    s[i] = getc();
    if (s[i] == '\n')
      break;
  }

  if (i == 0)
    return NULL;

  s[i] = '\0';
  return s;
}
