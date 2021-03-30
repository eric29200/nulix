#include <lib/stdio.h>
#include <lib/stdarg.h>
#include <drivers/screen.h>

static char *__buf_ptr;

/*
 * Put a character to a file descriptor.
 */
static void putc(char c, int fd)
{
  if (fd == stdout)
    screen_putc(c);
}

/*
 * Put a character into the temp buffer.
 */
static void __putc_buf(char c, int fd)
{
  UNUSED(fd);
  *__buf_ptr++ = c;
}

/*
 * Print a formatted signed number to a file descriptor.
 */
static int __print_num_signed(void (*putch)(char, int), int fd, int32_t num, uint16_t base)
{
  static char *digits = "0123456789abcdef";
  bool is_negative = FALSE;
  char buf[16];
  int32_t n = num;
  int i, ret;

  if (num < 0) {
    n = -n;
    is_negative = TRUE;
  }

  i = 0;
  do {
    buf[i++] = digits[n % base];
    n /= base;
  } while (n > 0);

  if (is_negative)
    buf[i++] = '-';

  ret = i;
  while (i >= 0)
    putch(buf[--i], fd);

  return ret;
}

/*
 * Print a formatted unsigned number to a file descriptor.
 */
static int __print_num_unsigned(void (*putch)(char, int), int fd, uint32_t num, uint16_t base)
{
  static char *digits = "0123456789abcdef";
  char buf[16];
  uint32_t n = num;
  int i, ret;

  i = 0;
  do {
    buf[i++] = digits[n % base];
    n /= base;
  } while (n > 0);

  ret = i;
  while (i >= 0)
    putch(buf[--i], fd);

  return ret;
}

/*
 * Formatted print into a file descriptor.
 */
static int vsprintf(void (*putch)(char, int), int fd, const char *format, va_list args)
{
  char *substr;
  int i, count;
  char c;

  for (i = 0, count = 0; format[i] != '\0'; i++) {
    c = format[i];

    if (c != '%') {
      putc(c, fd);
      count++;
      continue;
    }

    c = format[++i];
    switch (c) {
      case 'c':
        putch(va_arg(args, char), fd);
        count++;
        break;
      case 'd':
      case 'i':
        count += __print_num_signed(putch, fd, va_arg(args, int32_t), 10);
        break;
      case 'u':
        count += __print_num_unsigned(putch, fd, va_arg(args, int32_t), 10);
        break;
      case 'x':
        putch('0', fd);
        putch('x', fd);
        count += 2;
        count += __print_num_unsigned(putch, fd, va_arg(args, uint32_t), 16);
        break;
      case 's':
        for (substr = va_arg(args, char *); *substr != '\0'; substr++, count++)
          putch(*substr, fd);
        break;
      case '%':
        putch('%', fd);
        count++;
        break;
      default:
        putch('%', fd);
        putch(c, fd);
        count += 2;
    }
  }

  return count;
}

/*
 * Formatted print into a file descriptor.
 */
int fprintf(int fd, const char *format, ...)
{
  va_list args;
  int ret;

  va_start(args, format);
  ret = vsprintf(putc, fd, format, args);
  va_end(args);

  return ret;
}

/*
 * Formatted print into a string.
 */
int sprintf(char *s, const char *format, ...)
{
  va_list args;
  int ret;

  va_start(args, format);
  __buf_ptr = s;
  ret = vsprintf(__putc_buf, -1, format, args);
  *__buf_ptr = '\0';
  va_end(args);

  return ret;
}

/*
 * Formatted print to standard output.
 */
int printf(const char *format, ...)
{
  va_list args;
  int ret;

  va_start(args, format);
  ret = vsprintf(putc, stdout, format, args);
  va_end(args);

  return ret;
}

/*
 * Panic.
 */
void panic(const char *message)
{
  printf("[PANIC] %s\n", message);
  for (;;);
}
