#include <stdio.h>

/*
 * Print a formatted string.
 */
int vsprintf(char *str, const char *format, va_list arg)
{
  return vsnprintf(str, STDIO_BUF_SIZE, format, arg);
}
