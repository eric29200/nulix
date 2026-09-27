#include <stdio.h>
#include <stdarg.h>
#include <stderr.h>
#include <proc/sched.h>
#include <lib/console.h>

#define LOG_BUF_LEN			16384
#define LOG_BUF_MASK			(LOG_BUF_LEN - 1)

#define SYSLOG_ACTION_CLOSE		0
#define SYSLOG_ACTION_OPEN		1
#define SYSLOG_ACTION_READ		2
#define SYSLOG_ACTION_READ_ALL		3
#define SYSLOG_ACTION_READ_CLEAR	4
#define SYSLOG_ACTION_CLEAR		5
#define SYSLOG_ACTION_CONSOLE_OFF	6
#define SYSLOG_ACTION_CONSOLE_ON	7
#define SYSLOG_ACTION_CONSOLE_LEVEL	8
#define SYSLOG_ACTION_SIZE_UNREAD	9
#define SYSLOG_ACTION_SIZE_BUFFER	10

/* mix variables */
static char __buf[1024];

/* consoles */
static LIST_HEAD(console_drivers);

/* syslog variables */
static char log_buf[LOG_BUF_LEN];
static int log_start = 0;
static int log_end = 0;
static int logged_chars = 0;
static DECLARE_WAIT_QUEUE_HEAD(log_wait);

#define LOG_BUF(idx)			(log_buf[(idx) & LOG_BUF_MASK])

/*
 * Syslog system call.
 */
int sys_syslog(int type, char *buf, int len)
{
	int count, i;

	switch (type) {
		case SYSLOG_ACTION_CLOSE:
			break;
		case SYSLOG_ACTION_OPEN:
			break;
		case SYSLOG_ACTION_READ:
			/* check input parameters */
			if (!buf || len < 0)
				return -EINVAL;
			if (!len)
				break;

			/* wait for log */
			while (log_start == log_end)
				sleep_on(&log_wait);

			/* read from ring buffer */
			for (i = 0; i < len && log_start != log_end; i++) {
				*buf++ = LOG_BUF(log_start);
				log_start++;
			}

			return i;
		case SYSLOG_ACTION_READ_ALL:
			/* check input parameters */
			if (!buf || len < 0)
				return -EINVAL;
			if (!len)
				break;

			/* compute length to read */
			count = len;
			if (count > LOG_BUF_LEN)
				count = LOG_BUF_LEN;
			if (count > logged_chars)
				count = logged_chars;

			/* read syslog */
			for (i = 0; i < count; i++)
				*buf++ = log_buf[(log_start + i) & LOG_BUF_MASK];

			return i;
		case SYSLOG_ACTION_SIZE_UNREAD:
			return log_end - log_start;
		case SYSLOG_ACTION_SIZE_BUFFER:
			return LOG_BUF_LEN;
		default:
			printk("sys_syslog: unknown type %d\n", type);
			return -EINVAL;
	}

	return 0;
}

/*
 * Emit a character in syslog buffer.
 */
static void emit_log_char(char c)
{
	LOG_BUF(log_end) = c;
	log_end++;
	if (log_end - log_start > LOG_BUF_LEN)
		log_start = log_end - LOG_BUF_LEN;
	if (logged_chars < LOG_BUF_LEN)
		logged_chars++;
}

/*
 * Print a formatted string.
 */
int vprintf(const char *fmt, va_list ap)
{
	struct list_head *pos;
	struct console *co;
	char *p, *buf_end;
	int i;

	/* print log level */
	i = sprintf(__buf, "<%s>", KERN_INFO);

	/* print time */
	i += sprintf(__buf + i, "[%5llu.%06llu] ", xtimes.tv_sec, xtimes.tv_nsec / 1000);

	/* print message in tmp buf */
	i += vsnprintf(__buf + i, sizeof(__buf), fmt, ap);
	buf_end = __buf + i;

	/* write message to syslog */
	for (p = __buf; p < buf_end; p++)
		emit_log_char(*p);

	/* write message to registered consoles */
	list_for_each(pos, &console_drivers) {
		co = list_entry(pos, struct console, list);
		if ((co->flags & CON_ENABLED) && co->write)
			co->write(co, __buf + 3, buf_end - __buf - 3);
	}

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

/*
 * Register a console driver.
 */
void register_console(struct console *co)
{
	int p, first_line = 1, line_start = 0;
	char c;

	/* init console */
	if (co->index < 0)
		co->index = 0;
	if (!co->setup || co->setup(co) == 0)
		co->flags |= CON_ENABLED;

	/* console disabled */
	if (!(co->flags & CON_ENABLED))
		return;

	/* add console */
	list_add_tail(&co->list, &console_drivers);

	/* print buffered log messages */
	p = log_start & LOG_BUF_MASK;
	while (p != log_end) {
		c = log_buf[p];
		p = (p + 1) & LOG_BUF_MASK;

		/* skip first truncated message */
		if (c != '\n' && first_line)
			continue;

		/* new line : print '\n' if not first line */
		if (c == '\n') {
			line_start = 1;
			if (first_line)
				first_line = 0;
			else
				co->write(co, &c, 1);
			continue;
		}

		/* line start : skip 3 first characters = log level */
		if (line_start) {
			line_start = line_start == 3 ? 0 : line_start + 1;
			continue;
		}

		/* write to console */
		co->write(co, &c, 1);
	}
}