#include <stdio.h>
#include <stdarg.h>
#include <stderr.h>
#include <proc/sched.h>
#include <drivers/char/serial.h>

#define LOG_BUF_LEN		8192

/* mix variables */
static char __buf[1024];

/* syslog variables */
static char log_buf[LOG_BUF_LEN];
static int log_start = 0;
static int log_size = 0;
static DECLARE_WAIT_QUEUE_HEAD(log_wait);

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

			return i;
		case 6:				/* disable logging to console */
			return 0;
		case 7:				/* enable logging to console */
			return 0;
		default:
			break;
	}

	printk("Unknown syslog %d\n", type);
	return -EINVAL;
}

/*
 * Print a formatted string.
 */
int vprintf(const char *fmt, va_list ap)
{
	int i, j;

	/* print in tmp buf */
	i = vsnprintf(__buf, sizeof(__buf), fmt, ap);

	/* write tmp buf to serial line */
	for (j = 0; j < i; j++)
		write_serial(__buf[j]);

	return i;
}

/*
 * Print a formatted string.
 */
int printk(const char *fmt, ...)
{
	va_list args;
	int ret;

	/* print in tmp buf */
	va_start(args, fmt);
	ret = vprintf(fmt, args);
	va_end(args);

	return ret;
}

/*
 * Panic.
 */
void panic(const char *fmt, ...)
{
	static char buf[1024];
	va_list args;

	va_start(args, fmt);
	vsprintf(buf, fmt, args);
	va_end(args);
	printk("[PANIC] %s\n", buf);

	/* infinite loop */
	for (;;);
}