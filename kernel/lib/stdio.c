#include <stdio.h>
#include <stdarg.h>
#include <stderr.h>
#include <proc/sched.h>
#include <drivers/char/serial.h>

#define LOG_BUF_LEN		8192

/* syslog */
static char log_buf[LOG_BUF_LEN];
static int log_start = 0;
static int log_size = 0;
static struct wait_queue *log_wait = NULL;

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
