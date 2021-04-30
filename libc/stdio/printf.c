#include <stdio.h>

/*
 * Print a formatted string.
 */
int printf(const char *format, ...)
{
  va_list arg;
  va_start(arg, format);
  return vprintf(format, arg);
}
