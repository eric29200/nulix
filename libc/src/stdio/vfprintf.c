#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include "__stdio_impl.h"

#define PRINT_BUFSIZ		64

#define FLAG_ZERO		(1 << 0)
#define FLAG_MINUS		(1 << 1)

#define LENGTH_SHORT_SHORT	1
#define LENGTH_SHORT		2
#define LENGTH_DEFAULT		3
#define LENGTH_LONG		4
#define LENGTH_LONG_LONG	5
#define LENGTH_INTMAX_T		6
#define LENGTH_SIZE_T		7

/*
 * Output characters.
 */
static int __out(FILE *fp, const char *str, size_t len)
{
	if (!ferror(fp))
		__fwritex((const unsigned char *) str, len, fp);

	return len;
}

/*
 * Print a string.
 */
static int prints(FILE *fp, const char *str, int width, int precision, int flags)
{
	size_t len = strlen(str);
	char padchar;
	int n = 0;

	/* set padchar */
	padchar = flags & FLAG_ZERO ? '0' : ' ';

	/* fix len */
	if (precision > 0 && (size_t) precision < len)
		len = precision;

	/* fix width */
	width -= len;

	/* pad left */
	if (!(flags & FLAG_MINUS))
		for (; width > 0; width--)
			n += __out(fp, &padchar, 1);

	/* print string */
	n += __out(fp, str, len);

	/* pad right */
	for (; width > 0; --width)
		n += __out(fp, &padchar, 1);

	return n;
}

/*
 * Print a number.
 */
static int printi(FILE *fp, intmax_t i, int base, int sign, int width, int flags, bool uppercase)
{
	char buf[PRINT_BUFSIZ], *s;
	int neg = 0, n = 0;
	intmax_t remainder; 
	char minus = '-';
	uintmax_t u = i;

	/* print zero */
	if (i == 0) {
		buf[0] = '0';
		buf[1] = 0;
		return prints(fp, buf, width, -1, flags);
	}

	/* negative value */
	if (sign && base == 10 && i < 0) {
		neg = 1;
		u = -i;
	}

	/* end buffer */
	s = buf + PRINT_BUFSIZ - 1;
	*s = 0;

	/* print digits */
	while (u) {
		remainder = u % base;
		if (remainder >= 10)
			remainder += (uppercase ? 'A' : 'a') - '0' - 10;

		*--s = remainder + '0';
		u /= base;
	}

	/* add negative sign */
	if (neg) {
		if (width & (flags & FLAG_ZERO)) {
			n += __out(fp, &minus, 1);
			width--;
		} else {
			*--s = minus;
		}
	}

	/* print digits */
	return n + prints(fp, s, width, -1, flags);
}

/*
 * Get a parameter value.
 */
static inline intmax_t __get_val(va_list *ap, int length_modifier)
{
	switch (length_modifier) {
		case LENGTH_SHORT_SHORT:
		case LENGTH_SHORT:
		case LENGTH_DEFAULT:
			return va_arg(*ap, int);
		case LENGTH_LONG:
			return va_arg(*ap, long);
		case LENGTH_LONG_LONG:
			return va_arg(*ap, long long);
		case LENGTH_INTMAX_T:
			return va_arg(*ap, intmax_t);
		case LENGTH_SIZE_T:
			return va_arg(*ap, ssize_t);
		default:
			return 0;
	}
}

/*
 * Print a formatted string.
 */
static int printf_core(FILE *fp, const char *format, va_list ap)
{
	int n = 0, i, width, precision, flags, length_modifier;
	char c[2];

	while (*format) {
		/* handle character */
		if (*format != '%') {
			for (i = 0; format[i] && format[i] != '%'; i++);
			n += __out(fp, format, i);
			format += i;
			continue;
		}

		/* %% = % character */
		if (*++format == '%') {
			n += __out(fp, format++, 1);
			continue;
		}

		/* handle flags */
		for (flags = 0;;) {
			switch (*format++) {
				case '0':
					flags |= FLAG_ZERO;
					continue;
				case '-':
					flags |= FLAG_MINUS;
					continue;
				case '#':
				case ' ':
				case '+':
				case '\'':
				case 'I':
					continue;
				default:
					format--;
					break;
			}

			break;
		}

		/* get width */
		for (width = 0; *format >= '0' && *format <= '9'; format++)
			width = width * 10 + *format - '0';

		/* get precision */
		if (*format == '.') {
			format++;

			for (precision = 0; *format >= '0' && *format <= '9'; format++)
				precision = precision * 10 + *format - '0';
		} else {
			precision = -1;
		}

		/* end of string */
		if (!*format)
			goto out;

		/* handle length modifier */
		length_modifier = LENGTH_DEFAULT;
		if (*format == 'h' && *(format + 1) == 'h') {
			length_modifier = LENGTH_SHORT_SHORT;
			format += 2;
		} else if (*format == 'l' && *(format + 1) == 'l') {
			length_modifier = LENGTH_LONG_LONG;
			format += 2;
		} else if (*format == 'h') {
			length_modifier = LENGTH_SHORT;
			format++;
		} else if (*format == 'l') {
			length_modifier = LENGTH_LONG;
			format++;
		} else if (*format == 'j') {
			length_modifier = LENGTH_INTMAX_T;
			format++;
		} else if (*format == 'z') {
			length_modifier = LENGTH_SIZE_T;
			format++;
		}

		/* handle type */
		switch (*format++) {
			case 0:
				goto out;
			case 'c':
			 	c[0] = va_arg(ap, int);
				c[1] = 0;
				n += prints(fp, c, width, precision, flags);
				break;
			case 's':
				n += prints(fp, va_arg(ap, char *), width, precision, flags);
				break;
			case 'd':
			case 'i':
				n += printi(fp, __get_val(&ap, length_modifier), 10, 1, width, flags, false);
				break;
			case 'u':
				n += printi(fp, __get_val(&ap, length_modifier), 10, 0, width, flags, false);
				break;
			case 'x':
				n += printi(fp, __get_val(&ap, length_modifier), 16, 0, width, flags, false);
				break;
			case 'X':
				n += printi(fp, __get_val(&ap, length_modifier), 16, 0, width, flags, true);
				break;
			default:
				break;
		}
	}

out:
	return n;
}

int vfprintf(FILE *fp, const char *format, va_list ap)
{
	int ret;

	ret = printf_core(fp, format, ap);
	if (ferror(fp))
		return -1;

	return ret;
}
