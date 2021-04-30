#include <stdio.h>

/*
 * Print a formatted string to a string.
 */
int snprintf(char *str, size_t size, const char *format, ...)
{
  va_list args;
  va_start(args, format);
  return vsnprintf(str, size, format, args);
}
