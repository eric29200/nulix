#include <stdio.h>
#include <stdarg.h>
#include <stderr.h>
#include <proc/sched.h>
#include <drivers/char/serial.h>

#define LOG_BUF_LEN		8192
#define QUALIFIER_LONG		1
#define QUALIFIER_LONG_LONG	2

#define FORMAT_TYPE_NONE		1
#define FORMAT_TYPE_CHAR		2
#define FORMAT_TYPE_STR			3
#define FORMAT_TYPE_PERCENT_CHAR	4
#define FORMAT_TYPE_INVALID		5
#define FORMAT_TYPE_LONG_LONG		6
#define FORMAT_TYPE_ULONG		7
#define FORMAT_TYPE_LONG		8
#define FORMAT_TYPE_UINT		9
#define FORMAT_TYPE_INT			10

#define SIGN				2

/*
 * Print specifications.
 */
struct printf_spec {
	uint8_t		type;
	uint8_t		flags;
	uint8_t		qualifier;
	uint8_t		base;
};

static char *__buf_ptr;

/* syslog */
static char log_buf[LOG_BUF_LEN];
static int log_start = 0;
static int log_size = 0;
static struct wait_queue *log_wait = NULL;

/*
 * Put a character into the temp buffer.
 */
static void __putc_buf(char c)
{
	*__buf_ptr++ = c;
}

/*
 * Print a formatted signed number to a file descriptor.
 */
static int __print_num_signed(void (*putch)(char), int64_t num, struct printf_spec *spec)
{
	static char *digits = "0123456789abcdef";
	int is_negative = 0;
	int64_t n = num;
	char buf[66];
	int i, ret;

	if (num < 0) {
		n = -n;
		is_negative = 1;
	}

	i = 0;
	do {
		buf[i++] = digits[n % spec->base];
		n /= spec->base;
	} while (n > 0);

	if (is_negative)
		buf[i++] = '-';

	if (spec->base == 16) {
		buf[i++] = 'x';
		buf[i++] = '0';
	}

	ret = i;
	while (i > 0)
		putch(buf[--i]);

	return ret;
}

/*
 * Print a formatted unsigned number to a file descriptor.
 */
static int __print_num_unsigned(void (*putch)(char), uint64_t num, struct printf_spec *spec)
{
	static char *digits = "0123456789abcdef";
	uint64_t n = num;
	char buf[66];
	int i, ret;

	i = 0;
	do {
		buf[i++] = digits[n % spec->base];
		n /= spec->base;
	} while (n > 0);

	if (spec->base == 16) {
		buf[i++] = 'x';
		buf[i++] = '0';
	}

	ret = i;
	while (i > 0)
		putch(buf[--i]);

	return ret;
}

/*
 * Decode format.
 */
static int format_decode(const char *fmt, struct printf_spec *spec)
{
	const char *start = fmt;

	/* default specifications */
	spec->type = FORMAT_TYPE_NONE;
	spec->flags = 0;
	spec->qualifier = -1;
	spec->base = 10;

	/* normal character */
	if (*fmt++ != '%')
		return 0;

	/* long qualifier */
	if (*fmt == 'l') {
		spec->qualifier = *fmt++;

		/* long long qualifier */
		if (*fmt == 'l') {
			spec->qualifier = 'L';
			fmt++;
		}
	}

	/* type */
	switch (*fmt) {
		case 'c':
			spec->type = FORMAT_TYPE_CHAR;
			return ++fmt - start;
		case 's':
			spec->type = FORMAT_TYPE_STR;
			return ++fmt - start;
		case '%':
			spec->type = FORMAT_TYPE_PERCENT_CHAR;
			return ++fmt - start;
		case 'd':
		case 'i':
			spec->flags |= SIGN;
			break;
		case 'u':
			break;
		case 'x':
			spec->base = 16;
			break;
		default:
			spec->type = FORMAT_TYPE_INVALID;
			return ++fmt - start;
	}

	/* adjust type */
	if (spec->qualifier == 'L')
		spec->type = FORMAT_TYPE_LONG_LONG;
	else if (spec->qualifier == 'l' && spec->flags & SIGN)
		spec->type = FORMAT_TYPE_LONG;
	else if (spec->qualifier == 'l')
		spec->type = FORMAT_TYPE_ULONG;
	else if (spec->flags & SIGN)
		spec->type = FORMAT_TYPE_INT;
	else
		spec->type = FORMAT_TYPE_UINT;

	return ++fmt - start;
}

/*
 * Formatted print into a file descriptor.
 */
static int vsprintf(void (*putch)(char), const char *format, va_list args)
{
	struct printf_spec spec;
	char *substr;
	int count = 0;

	while (*format) {
		/* decode format */
		int read = format_decode(format, &spec);

		format += read;

		/* format */
		switch (spec.type) {
			case FORMAT_TYPE_NONE:
				putch(*format++);
				count++;
				break;
			case FORMAT_TYPE_CHAR:
				putch(va_arg(args, int));
				count++;
				break;
			case FORMAT_TYPE_STR:
				for (substr = va_arg(args, char *); *substr != '\0'; substr++, count++)
					putch(*substr);
				break;
			case FORMAT_TYPE_PERCENT_CHAR:
				putch('%');
				count++;
				break;
			case FORMAT_TYPE_INVALID:
				putch('%');
				count++;
				break;
			case FORMAT_TYPE_LONG_LONG:
				count += __print_num_signed(putch, va_arg(args, int64_t), &spec);
				break;
			case FORMAT_TYPE_ULONG:
				count += __print_num_unsigned(putch, va_arg(args, uint32_t), &spec);
				break;
			case FORMAT_TYPE_LONG:
				count += __print_num_signed(putch, va_arg(args, int32_t), &spec);
				break;
			case FORMAT_TYPE_UINT:
				count += __print_num_unsigned(putch, va_arg(args, uint32_t), &spec);
				break;
			default:
				count += __print_num_signed(putch, va_arg(args, int32_t), &spec);
				break;
		}
	}

	return count;
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
	ret = vsprintf(__putc_buf, format, args);
	*__buf_ptr = 0;
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
	ret = vsprintf(write_serial, format, args);
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

/*
 * Syslog system call.
 */
int sys_syslog(int type, char *buf, int len)
{
	int i;

	switch (type) {
		case 0:				/* close */
			return 0;
		case 1:				/* open */
			return 0;
		case 2:				/* read from log */
			/* check input parameters */
			if (!buf || len < 0)
				return -EINVAL;

			/* wait for log */
			while (!log_size)
				sleep_on(&log_wait);

			/* read from ring buffer */
			for (i = 0; i < len && log_size > 0; i++) {
				*buf++ = log_buf[log_start++];
				log_size--;
				log_start &= LOG_BUF_LEN - 1;
			}

			printf("%d\n", i);
			return i;
		case 6:				/* disable logging to console */
			return 0;
		case 7:				/* enable logging to console */
			return 0;
		default:
			break;
	}

	printf("Unknown syslog %d\n", type);
	return -EINVAL;
}
