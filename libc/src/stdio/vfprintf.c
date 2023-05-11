#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include "__stdio_impl.h"

#define PRINT_BUFSIZ		64

#define FLAG_ZERO		(1 << 0)
#define FLAG_MINUS		(1 << 1)

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
static int prints(FILE *fp, const char *str, int width, int flags)
{
	size_t len = strlen(str);
	char padchar;
	int n = 0;

	/* set padchar */
	padchar = flags & FLAG_ZERO ? '0' : ' ';

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
static int printi(FILE *fp, int i, int base, int sign, int width, int flags, bool uppercase)
{
	int neg = 0, remainder, n = 0;
	char buf[PRINT_BUFSIZ], *s;
	unsigned int u = i;
	char minus = '-';

	/* print zero */
	if (i == 0) {
		buf[0] = '0';
		buf[1] = 0;
		return prints(fp, buf, width, flags);
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
	return n + prints(fp, s, width, flags);
}

/*
 * Print a formatted string.
 */
static int printf_core(FILE *fp, const char *format, va_list ap)
{
	int n = 0, i, width, flags;
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
			n += __out(fp, format, 1);
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

		/* end of string */
		if (!*format)
			goto out;

		/* handle type */
		switch (*format++) {
			case 0:
				goto out;
			case 'c':
			 	c[0] = va_arg(ap, int);
				c[1] = 0;
				n += prints(fp, c, width, flags);
				break;
			case 's':
				n += prints(fp, va_arg(ap, char *), width, flags);
				break;
			case 'd':
			case 'i':
				n += printi(fp, va_arg(ap, int), 10, 1, width, flags, false);
				break;
			case 'u':
				n += printi(fp, va_arg(ap, unsigned int), 10, 0, width, flags, false);
				break;
			case 'x':
				n += printi(fp, va_arg(ap, unsigned int), 16, 0, width, flags, false);
				break;
			case 'X':
				n += printi(fp, va_arg(ap, unsigned int), 16, 0, width, flags, true);
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