#include <stdio.h>
#include <stdarg.h>
#include <stderr.h>
#include <proc/sched.h>
#include <drivers/char/serial.h>

#define LOG_BUF_LEN		8192
#define QUALIFIER_LONG		1
#define QUALIFIER_LONG_LONG	2

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
static int __print_num_signed(void (*putch)(char), int32_t num, uint16_t base)
{
	static char *digits = "0123456789abcdef";
	int is_negative = 0;
	int32_t n = num;
	char buf[16];
	int i, ret;

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
	while (i > 0)
		putch(buf[--i]);

	return ret;
}

/*
 * Print a formatted signed number to a file descriptor.
 */
static int __print_num_signed_64(void (*putch)(char), int64_t num, uint16_t base)
{
	static char *digits = "0123456789abcdef";
	int is_negative = 0;
	int64_t n = num;
	char buf[16];
	int i, ret;

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
	while (i > 0)
		putch(buf[--i]);

	return ret;
}

/*
 * Print a formatted unsigned number to a file descriptor.
 */
static int __print_num_unsigned_64(void (*putch)(char), uint64_t num, uint16_t base)
{
	static char *digits = "0123456789abcdef";
	uint64_t n = num;
	char buf[32];
	int i, ret;

	i = 0;
	do {
		buf[i++] = digits[n % base];
		n /= base;
	} while (n > 0);

	ret = i;
	while (i > 0)
		putch(buf[--i]);

	return ret;
}

/*
 * Print a formatted unsigned number to a file descriptor.
 */
static int __print_num_unsigned(void (*putch)(char), uint32_t num, uint16_t base)
{
	static char *digits = "0123456789abcdef";
	uint32_t n = num;
	char buf[32];
	int i, ret;

	i = 0;
	do {
		buf[i++] = digits[n % base];
		n /= base;
	} while (n > 0);

	ret = i;
	while (i > 0)
		putch(buf[--i]);

	return ret;
}

/*
 * Formatted print into a file descriptor.
 */
static int vsprintf(void (*putch)(char), const char *format, va_list args)
{
	int count = 0, qualifier = QUALIFIER_LONG;
	char *substr, c;

	while (*format) {
		c = *format++;

		/* normal character */
		if (c != '%') {
			putch(c);
			count++;
			continue;
		}

		/* get next character */
		c = *format++;

		/* long qualifier */
		if (c == 'l') {
			qualifier = QUALIFIER_LONG;
			c = *format++;
		
			/* long long qualifier */
			if (c == 'l') {
				qualifier = QUALIFIER_LONG_LONG;
				c = *format++;
			}
		}

		/* format */
		switch (c) {
			case 'c':
				putch(va_arg(args, int));
				count++;
				break;
			case 'd':
			case 'i':
				if (qualifier == QUALIFIER_LONG_LONG)
					count += __print_num_signed_64(putch, va_arg(args, int64_t), 10);
				else
					count += __print_num_signed(putch, va_arg(args, int32_t), 10);
				break;
			case 'u':
				if (qualifier == QUALIFIER_LONG_LONG)
					count += __print_num_unsigned_64(putch, va_arg(args, uint64_t), 10);
				else
					count += __print_num_unsigned(putch, va_arg(args, uint32_t), 10);
				break;
			case 'x':
				putch('0');
				putch('x');
				count += 2;
				if (qualifier == QUALIFIER_LONG_LONG)
					count += __print_num_unsigned_64(putch, va_arg(args, uint64_t), 16);
				else
					count += __print_num_unsigned(putch, va_arg(args, uint32_t), 16);
				break;
			case 's':
				for (substr = va_arg(args, char *); *substr != '\0'; substr++, count++)
					putch(*substr);
				break;
			case '%':
				putch('%');
				count++;
				break;
			default:
				putch('%');
				putch(c);
				count += 2;
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
