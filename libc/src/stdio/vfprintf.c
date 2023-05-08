#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdbool.h>

#include "__stdio_impl.h"

/*
 * Print a formatted signed number to a file descriptor.
 */
static int __print_num_signed(FILE *fp, int num, int base)
{
	static char *digits = "0123456789abcdef";
	bool is_negative = false;
	int i, ret, n = num;
	char buf[16];

	if (num < 0) {
		n = -n;
		is_negative = true;
	}

	i = 0;
	do {
		buf[i++] = digits[n % base];
		n /= base;
	} while (n > 0);

	if (is_negative)
		buf[i++] = '-';

	ret = i;
	while (i > 0)
		putc(buf[--i], fp);

	return ret;
}

/*
 * Print a formatted unsigned number to a file descriptor.
 */
static int __print_num_unsigned(FILE *fp, unsigned int num, int base)
{
	static char *digits = "0123456789abcdef";
	unsigned int n = num;
	char buf[16];
	int i, ret;

	i = 0;
	do {
		buf[i++] = digits[n % base];
		n /= base;
	} while (n > 0);

	ret = i;
	while (i > 0)
		putc(buf[--i], fp);

	return ret;
}

/*
 * Print a formatted string.
 */
static int printf_core(FILE *fp, const char *format, va_list args)
{
	char *substr;
	int i, count;
	char c;

	for (i = 0, count = 0; format[i] != '\0'; i++) {
		c = format[i];

		if (c != '%') {
			putc(c, fp);
			count++;
			continue;
		}

		c = format[++i];
		switch (c) {
			case 'c':
				putc(va_arg(args, int), fp);
				count++;
				break;
			case 'd':
			case 'i':
				count += __print_num_signed(fp, va_arg(args, int), 10);
				break;
			case 'u':
				count += __print_num_unsigned(fp, va_arg(args, unsigned int), 10);
				break;
			case 'x':
				putc('0', fp);
				putc('x', fp);
				count += 2;
				count += __print_num_unsigned(fp, va_arg(args, unsigned int), 16);
				break;
			case 's':
				for (substr = va_arg(args, char *); *substr != '\0'; substr++, count++)
					putc(*substr, fp);
				break;
			case '%':
				putc('%', fp);
				count++;
				break;
			default:
				putc('%', fp);
				putc(c, fp);
				count += 2;
				break;
		}
	}

	return count;
}

int vfprintf(FILE *fp, const char *format, va_list ap)
{
	int ret;
	
	ret = printf_core(fp, format, ap);
	if (ferror(fp))
		return -1;

	return ret;
}