#include <drivers/char/mouse.h>
#include <x86/interrupt.h>
#include <x86/io.h>
#include <proc/sched.h>
#include <sys/syscall.h>
#include <fs/fs.h>
#include <stdio.h>
#include <stderr.h>
#include <fcntl.h>
#include <dev.h>

#define MOUSE_BUF_SIZE		2048
#define MOUSE_IRQ		12
#define MOUSE_PORT		0x60
#define MOUSE_STATUS		0x64
#define MOUSE_COMMAND		0x64
#define MOUSE_CMD_WRITE		0x60
#define MOUSE_MAGIC_WRITE	0xD4
#define MOUSE_OBUF_FULL		0x21
#define MOUSE_INTS_ON		0x47
#define MOUSE_INTS_OFF		0x65
#define MOUSE_DISABLE		0xA7
#define MOUSE_ENABLE		0xA8
#define MOUSE_SET_RES		0xE8
#define MOUSE_SET_SCALE11	0xE6
#define MOUSE_SET_SCALE21	0xE7
#define MOUSE_SET_SAMPLE	0xF3
#define MOUSE_ENABLE_DEV	0xF4

#define MAX_RETRIES		1000000

/* mouse static variables */
static uint8_t queue_buf[MOUSE_BUF_SIZE];
static int queue_head = 0;
static int queue_tail = 0;
static struct wait_queue *queue_wait = NULL;
static int mouse_count = 0;
static int mouse_ready = 0;

/*
 * Is mouse queue empty ?
 */
static inline int queue_empty()
{
	return queue_head == queue_tail;
}

/*
 * Get next character from queue.
 */
static uint8_t get_from_queue()
{
	uint8_t ret;

	ret = queue_buf[queue_tail];
	queue_tail = (queue_tail + 1) & (MOUSE_BUF_SIZE - 1);

	return ret;
}

/*
 * Wait for mouse.
 */
static int mouse_poll_status()
{
	int retries = 0;

	while (inb(MOUSE_STATUS) & 0x03 && retries < MAX_RETRIES) {
		if ((inb(MOUSE_STATUS) & MOUSE_OBUF_FULL) == MOUSE_OBUF_FULL)
			inb(MOUSE_PORT);

		current_task->state = TASK_SLEEPING;
		schedule_timeout((5 * HZ + 99) / 100);
		retries++;
	}
	return retries != MAX_RETRIES;
}

/*
 * Wait for mouse.
 */
static int mouse_poll_status_no_sleep()
{
	int retries = 0;

	while (inb(MOUSE_STATUS) & 0x03 && retries < MAX_RETRIES) {
		if ((inb(MOUSE_STATUS) & MOUSE_OBUF_FULL) == MOUSE_OBUF_FULL)
			inb(MOUSE_PORT);

		retries++;
	}

	return retries != MAX_RETRIES;
}

/*
 * Write to mouse device and handle ack.
 */
static int mouse_write_ack(int value)
{
	/* write value */
	mouse_poll_status_no_sleep();
	outb(MOUSE_COMMAND, MOUSE_MAGIC_WRITE);
	mouse_poll_status_no_sleep();
	outb(MOUSE_PORT, value);
	mouse_poll_status_no_sleep();

	/* handle ack */
	if ((inb(MOUSE_STATUS & MOUSE_OBUF_FULL)) == MOUSE_OBUF_FULL)
		return inb(MOUSE_PORT);

	return 0;
}

/*
 * Write a command to mouse.
 */
static void mouse_write_cmd(int value)
{
	mouse_poll_status();
	outb(MOUSE_COMMAND, MOUSE_CMD_WRITE);
	mouse_poll_status();
	outb(MOUSE_PORT, value);
}

/*
 * Write to mouse.
 */
static void mouse_write_dev(int value)
{
	mouse_poll_status();
	outb(MOUSE_COMMAND, MOUSE_MAGIC_WRITE);
	mouse_poll_status();
	outb(MOUSE_PORT, value);
}

/*
 * Mouse interrupt handler.
 */
static void mouse_interrupt_handler(struct registers *regs)
{
	int max_head = (queue_tail - 1) & (MOUSE_BUF_SIZE - 1);

	/* unused registers */
	UNUSED(regs);

	/* check status */
	if ((inb(MOUSE_STATUS) & MOUSE_OBUF_FULL) != MOUSE_OBUF_FULL)
		return;

	/* queue data */
	queue_buf[queue_head] = inb(MOUSE_PORT);
	if (queue_head != max_head) {
		queue_head++;
		queue_head &= MOUSE_BUF_SIZE - 1;
	}

	/* mouse ready */
	mouse_ready = 1;

	/* wake up processes */
	wake_up(&queue_wait);
}

/*
 * Open mouse device.
 */
static int mouse_open(struct inode *inode, struct file *filp)
{
	UNUSED(inode);
	UNUSED(filp);

	/* mouse already installed */
	if (mouse_count++)
		return 0;

	/* reset queue */
	queue_head = queue_tail = 0;
	mouse_ready = 0;

	/* register interrupt */
	register_interrupt_handler(MOUSE_IRQ, mouse_interrupt_handler);

	/* enable mouse */
	mouse_poll_status();
	outb(MOUSE_COMMAND, MOUSE_ENABLE);
	mouse_write_dev(MOUSE_ENABLE_DEV);
	mouse_write_cmd(MOUSE_INTS_ON);
	mouse_poll_status();

	return 0;
}

/*
 * Close mouse device.
 */
static int mouse_release(struct inode *inode, struct file *filp)
{
	UNUSED(inode);
	UNUSED(filp);

	/* device still used */
	if (--mouse_count)
		return 0;

	/* disable mouse */
	mouse_write_cmd(MOUSE_INTS_OFF);
	mouse_poll_status();
	outb(MOUSE_COMMAND, MOUSE_DISABLE);
	mouse_poll_status();

	/* free interrupt */
	unregister_interrupt_handler(MOUSE_IRQ);

	return 0;
}

/*
 * Read mouse device.
 */
static int mouse_read(struct file *filp, char *buf, size_t n, off_t *ppos)
{
	size_t i;

	/* unused offset */
	UNUSED(ppos);

	/* if queue is empty, wait for data */
	if (queue_empty()) {
		/* non blocking : return */
		if (filp->f_flags & O_NONBLOCK)
			return -EAGAIN;

		/* signal pending : return */
		if (signal_pending(current_task))
			return -EINTR;

		/* wait for data */
		sleep_on(&queue_wait);
	}

	/* read from queue */
	for (i = 0; i < n && !queue_empty(); i++)
		buf[i] = get_from_queue();

	mouse_ready = !queue_empty();

	/* upate inode */
	if (i)
		update_atime(filp->f_dentry->d_inode);

	return i;
}

/*
 * Write to mouse device.
 */
static int mouse_write(struct file *filp, const char *buf, size_t n, off_t *ppos)
{
	UNUSED(filp);
	UNUSED(buf);
	UNUSED(n);
	UNUSED(ppos);
	return n;
}

/*
 * Poll mouse device.
 */
static int mouse_poll(struct file *filp, struct select_table *wait)
{
	int mask = 0;

	/* unused filp */
	UNUSED(filp);

	/* check if there is some characters to read */
	if (mouse_ready)
		mask |= POLLIN;

	/* add queue wait to select table */
	select_wait(&queue_wait, wait);

	return mask;
}

/*
 * Mouse file operations.
 */
static struct file_operations mouse_fops = {
	.open		= mouse_open,
	.release	= mouse_release,
	.read		= mouse_read,
	.write		= mouse_write,
	.poll		= mouse_poll,
};

/*
 * Init mouse device.
 */
int init_mouse()
{
	int ret;

	/* register mouse device */
	ret = register_chrdev(DEV_MOUSE_MAJOR, "mouse", &mouse_fops);
	if (ret)
		return ret;

	/* empty mouse input buffer */
	while (inb(MOUSE_STATUS) & 0x01)
		inb(MOUSE_PORT);

	/* setup mouse */
	outb(MOUSE_COMMAND, MOUSE_ENABLE);
	mouse_write_ack(MOUSE_SET_SAMPLE);
	mouse_write_ack(100);
	mouse_write_ack(MOUSE_SET_RES);
	mouse_write_ack(3);
	mouse_write_ack(MOUSE_SET_SCALE21);

	/* disable mouse */
	mouse_poll_status_no_sleep();
	outb(MOUSE_COMMAND, MOUSE_DISABLE);
	mouse_poll_status_no_sleep();
	outb(MOUSE_COMMAND, MOUSE_CMD_WRITE);
	mouse_poll_status_no_sleep();
	outb(MOUSE_PORT, MOUSE_INTS_OFF);

	return 0;
}
