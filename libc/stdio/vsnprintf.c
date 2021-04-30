#include <stdio.h>

/*
 * Print a formatted signed number to a file descriptor.
 */
static int __print_num_signed(char *str, int size, int32_t num, uint16_t base)
{
  static char *digits = "0123456789abcdef";
  int is_negative = 0;
  int32_t n = num;
  char buf[16];
  int i, j, ret;

  if (num < 0) {
    n = -n;
    is_negative = 1;
  }

  i = 0;
  do {
    buf[i++] = digits[n % base];
    n /= base;
  } while (n > 0);

  if (is_negative)
    buf[i++] = '-';

  ret = i;
  if (ret > size)
    return -1;

  j = 0;
  while (i >= 0)
    str[j++] = buf[--i];

  return ret;
}

/*
 * Print a formatted unsigned number to a file descriptor.
 */
static int __print_num_unsigned(char *str, int size, uint32_t num, uint16_t base)
{
  static char *digits = "0123456789abcdef";
  uint32_t n = num;
  char buf[16];
  int i, j, ret;

  i = 0;
  do {
    buf[i++] = digits[n % base];
    n /= base;
  } while (n > 0);

  ret = i;
  if (ret > size)
    return -1;

  j = 0;
  while (i >= 0)
    str[j++] = buf[--i];

  return ret;
}

/*
 * Formatted print into a file descriptor.
 */
int vsnprintf(char *str, size_t size, const char *format, va_list args)
{
  size_t count;
  char *substr;
  int i, ret;
  char c;

  for (i = 0, count = 0; format[i] != '\0' && count < size - 1; i++) {
    c = format[i];

    if (c != '%') {
      str[count++] = c;
      continue;
    }

    c = format[++i];
    switch (c) {
      case 'c':
        str[count++] = va_arg(args, char);
        break;
      case 'd':
      case 'i':
        ret = __print_num_signed(&str[count], size - 1 - count, va_arg(args, int32_t), 10);
        if (ret >= 0)
          count += ret;
        break;
      case 'u':
        ret = __print_num_unsigned(&str[count], size - 1 - count, va_arg(args, int32_t), 10);
        if (ret >= 0)
          count += ret;
        break;
      case 'x':
        str[count++] = '0';
        if (count >= size - 1)
          break;
        str[count++] = 'x';
        if (count >= size - 1)
          break;
        ret = __print_num_unsigned(&str[count], size - 1 - count, va_arg(args, uint32_t), 16);
        if (ret >= 0)
          count += ret;
        break;
      case 's':
        for (substr = va_arg(args, char *); *substr != '\0' && count < size - 1; substr++)
          str[count++] = *substr;
        break;
      case '%':
        str[count++] = '%';
        break;
      default:
        str[count++] = '%';
        if (count < size - 1)
          str[count++] = 'c';
        break;
    }
  }

  str[count] = '\0';
  return count;
}
