#include <stdio.h>
#include <unistd.h>

/*
 * Print a formatted string.
 */
int vprintf(const char *format, va_list arg)
{
  char buf[STDIO_BUF_SIZE];
  int n;

  n = vsnprintf(buf, STDIO_BUF_SIZE, format, arg);
  if (n < 0)
    return n;

  return write(STDOUT, buf, n);
}
