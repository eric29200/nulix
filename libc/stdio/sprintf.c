#include <stdio.h>

/*
 * Print a formatted string to a string.
 */
int sprintf(char *str, const char *format, ...)
{
  va_list args;
  va_start(args, format);
  return vsprintf(str, format, args);
}
