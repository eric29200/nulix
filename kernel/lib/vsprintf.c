#include <stdio.h>
#include <string.h>
#include <drivers/char/serial.h>

#define ZEROPAD		1
#define SIGN		2
#define PLUS		4
#define SPACE		8
#define LEFT		16
#define SPECIAL		32
#define LARGE		64

#define is_digit(c)	((c) >= '0' && (c) <= '9')

/*
 * Parse an int.
 */
static int skip_atoi(const char **s)
{
	int i = 0;

	while (is_digit(**s))
		i = i * 10 + *((*s)++) - '0';

	return i;
}

/*
 * Do division.
 */
static inline int do_div(long long *n, int base)
{
	int res = ((unsigned long long) *n) % (unsigned) base;
	*n = ((unsigned long long) *n) / (unsigned) base;
	return res;
}

/*
 * Print a number.
 */
static char *number(char *str, long long num, int base, int size, int precision, int type)
{
	const char *digits = "0123456789abcdefghijklmnopqrstuvwxyz";
	char c, sign = 0, tmp[66];
	int i = 0;

	/* uppercase digits */
	if (type & LARGE)
		digits = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";

	if (type & LEFT)
		type &= ~ZEROPAD;

	/* check base */
	if (base < 2 || base > 36)
		return 0;

	/* padding character */
	c = (type & ZEROPAD) ? '0' : ' ';

	/* sign */
	if (type & SIGN) {
		if (num < 0) {
			sign = '-';
			num = -num;
			size--;
		} else if (type & PLUS) {
			sign = '+';
			size--;
		} else if (type & SPACE) {
			sign = ' ';
			size--;
		}
	}

	/* special number */
	if (type & SPECIAL) {
		if (base == 16)
			size -= 2;
		else if (base == 8)
			size--;
	}

	/* print number in a tmp buffer */
	if (num == 0) {
		tmp[i++] = '0';
	} else {
		while (num != 0)
			tmp[i++] = digits[do_div(&num, base)];
	}

	/* limit to precision */
	if (i > precision)
		precision = i;
	size -= precision;


	if (!(type & (ZEROPAD + LEFT)))
		while (size-- > 0)
			*str++ = ' ';

	/* add sign */
	if (sign)
		*str++ = sign;

	/* special : add '0' for octal or '0x' for hexadecimal */
	if (type & SPECIAL) {
		if (base == 8) {
			*str++ = '0';
		} else if (base == 16) {
			*str++ = '0';
			*str++ = digits[33];
		}
	}

	/* left pad */
	if (!(type & LEFT))
		while (size-- > 0)
			*str++ = c;
	/* zero pad */
	while (i < precision--)
		*str++ = '0';

	/* print number */
	while (i-- > 0)
		*str++ = tmp[i];

	/* right pad */
	while (size-- > 0)
		*str++ = ' ';

	return str;
}

/*
 * Print a formatted string in a string.
 */
int vsnprintf(char *buf, int n, const char *fmt, va_list args)
{
	int len, i, base, flags, field_width, precision, qualifier;
	unsigned long long num;
	const char *s;
	char *str;

	for (str = buf; *fmt && (n == -1 || str - buf < n); fmt++) {
		/* normal character */
		if (*fmt != '%') {
			*str++ = *fmt;
			continue;
		}

		/* process flags */
		flags = 0;
repeat:
		fmt++;
		switch (*fmt) {
			case '-':
				flags |= LEFT;
				goto repeat;
			case '+':
				flags |= PLUS;
				goto repeat;
			case ' ':
				flags |= SPACE;
				goto repeat;
			case '#':
				flags |= SPECIAL;
				goto repeat;
			case '0':
				flags |= ZEROPAD;
				goto repeat;
		}

		/* get field width */
		field_width = -1;
		if (is_digit(*fmt)) {
			field_width = skip_atoi(&fmt);
		} else if (*fmt == '*') {
			fmt++;

			field_width = va_arg(args, int);
			if (field_width < 0) {
				field_width = -field_width;
				flags |= LEFT;
			}
		}

		/* get precision */
		precision = -1;
		if (*fmt == '.') {
			fmt++;
			if (is_digit(*fmt)) {
				precision = skip_atoi(&fmt);
			} else if (*fmt == '*') {
				fmt++;
				precision = va_arg(args, int);
			}

			if (precision < 0)
				precision = 0;
		}

		/* get the conversion qualifier */
		qualifier = -1;
		if (*fmt == 'h' || *fmt == 'l' || *fmt == 'L') {
			qualifier = *fmt++;

			if (qualifier == 'l' && *fmt == 'l') {
				qualifier = 'L';
				fmt++;
			}
		}

		/* default base */
		base = 10;
		switch (*fmt) {
			case 'c':
				if (!(flags & LEFT))
					while (--field_width > 0)
						*str++ = ' ';

				*str++ = (unsigned char) va_arg(args, int);

				while (--field_width > 0)
					*str++ = ' ';

				continue;

			case 's':
				s = va_arg(args, char *);
				if (!s)
					s = "<NULL>";

				len = strnlen(s, precision);

				if (n != -1 && len >= n - (str - buf)) {
					len = n - 1 - (str - buf);
					if (len <= 0)
						break;
					if (len < field_width)
						field_width = len;
				}

				if (!(flags & LEFT))
					while (len < field_width--)
						*str++ = ' ';

				for (i = 0; i < len; i++)
					*str++ = *s++;

				while (len < field_width--)
					*str++ = ' ';

				continue;
			case '%':
				*str++ = '%';
				continue;
			case 'o':
				base = 8;
				break;
			case 'X':
				flags |= LARGE;
				base = 16;
				break;
			case 'x':
				base = 16;
				break;
			case 'd':
			case 'i':
				flags |= SIGN;
			case 'u':
				break;
			default:
				*str++ = '%';
				if (*fmt)
					*str++ = *fmt;
				else
					fmt--;
				continue;
		}

		/* get number */
		if (qualifier == 'L') {
			num = va_arg(args, long long);
		} else if (qualifier == 'l') {
			num = va_arg(args, unsigned long);
			if (flags & SIGN)
				num = (long) num;
		} else if (qualifier == 'h') {
			num = (unsigned short) va_arg(args, int);
			if (flags & SIGN)
				num = (short) num;
		} else {
			num = va_arg(args, unsigned int);
			if (flags & SIGN)
				num = (int) num;
		}

		/* print number */
		str = number(str, num, base, field_width, precision, flags);
	}

	/* end string */
	*str = '\0';
	return str - buf;
}

/*
 * Print a formatted string in a string.
 */
int vsprintf(char *buf, const char *fmt, va_list args)
{
	return vsnprintf(buf, -1, fmt, args);
}

/*
 * Print a formatted string in a string.
 */
int snprintf(char *buf, size_t size, const char *fmt, ...)
{
	va_list args;
	int i;

	va_start(args, fmt);
	i = vsnprintf(buf, size, fmt, args);
	va_end(args);

	return i;
}

/*
 * Print a formatted string in a string.
 */
int sprintf(char *buf, const char *fmt, ...)
{
	va_list args;
	int i;

	va_start(args, fmt);
	i = vsprintf(buf, fmt, args);
	va_end(args);

	return i;
}

static char __buf[1024];

/*
 * Print a formatted string.
 */
int printf(const char *fmt, ...)
{
	va_list args;
	int i, j;

	/* print in tmp buf */
	va_start(args, fmt);
	i = vsnprintf(__buf, sizeof(__buf), fmt, args);
	va_end(args);

	/* write tmp buf to serial line */
	for (j = 0; j < i; j++)
		write_serial(__buf[j]);

	return i;
}
