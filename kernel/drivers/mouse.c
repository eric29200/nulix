#include <drivers/mouse.h>
#include <x86/interrupt.h>
#include <proc/sched.h>
#include <x86/io.h>
#include <lib/ring_buffer.h>
#include <fs/fs.h>
#include <stderr.h>

#define MOUSE_BUF_SIZE		2048
#define MOUSE_EVENT_SIZE	3

#define MOUSE_PORT		0x60
#define MOUSE_STATUS		0x64
#define MOUSE_COMMAND		0x64

#define MOUSE_CMD_WRITE		0x60
#define MOUSE_MAGIC_WRITE	0xD4

#define MOUSE_BBIT		0x01
#define MOUSE_FBIT		0x20
#define MOUSE_VBIT		0x08
#define MOUSE_BUTTON_LEFT	0x01
#define MOUSE_BUTTON_RIGHT	0x02
#define MOUSE_BUTTON_MIDDLE	0x04

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

/* mouse static variables */
static struct ring_buffer_t mouse_rb;
static uint8_t mouse_byte[MOUSE_EVENT_SIZE];
static int mouse_cycle = 0;

/*
 * Wait for mouse.
 */
static void mouse_poll_status_no_sleep()
{
	while (inb(MOUSE_STATUS) & 0x03)
		if ((inb(MOUSE_STATUS) & MOUSE_OBUF_FULL) == MOUSE_OBUF_FULL)
			inb(MOUSE_PORT);
}

/*
 * Write to mouse and handle ack.
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
	mouse_poll_status_no_sleep();
	outb(MOUSE_COMMAND, MOUSE_CMD_WRITE);
	mouse_poll_status_no_sleep();
	outb(MOUSE_PORT, value);
}

/*
 * Write to mouse.
 */
static void mouse_write_dev(int value)
{
	mouse_poll_status_no_sleep();
	outb(MOUSE_COMMAND, MOUSE_MAGIC_WRITE);
	mouse_poll_status_no_sleep();
	outb(MOUSE_PORT, value);
}

/*
 * Compute mouse event.
 */
static void mouse_compute_event()
{
	uint8_t state, event[MOUSE_EVENT_SIZE];
	int move_x, move_y, buttons;

	/* reset mouse cycle */
	mouse_cycle = 0;

	/* get mouse data */
	state = mouse_byte[0];
	move_x = mouse_byte[1];
	move_y = mouse_byte[2];

	/* apply x sign */
	if (move_x && (state & (1 << 4)))
		move_x -= 0x100;

	/* apply y sign */
	if (move_y && (state & (1 << 5)))
		move_y -= 0x100;

	/* overflow */
	if (state & (1 << 6) || state & (1 << 7)) {
		move_x = 0;
		move_y = 0;
	}

	/* set mouse event button */
	buttons = 0;
	if (state & MOUSE_BUTTON_LEFT)
		buttons |= MOUSE_BUTTON_LEFT;
	if (state & MOUSE_BUTTON_RIGHT)
		buttons |= MOUSE_BUTTON_RIGHT;
	if (state & MOUSE_BUTTON_MIDDLE)
		buttons |= MOUSE_BUTTON_MIDDLE;

	/* put mouse event in ring buffer */
	if (ring_buffer_left(&mouse_rb) >= MOUSE_EVENT_SIZE) {
		event[0] = buttons;
		event[1] = move_x;
		event[2] = move_y;
		ring_buffer_write(&mouse_rb, event, MOUSE_EVENT_SIZE);
	}
}

/*
 * Mouse interrupt handler.
 */
static void mouse_handler(struct registers_t *regs)
{
	int status, value;

	/* unused registers */
	UNUSED(regs);

	/* check status */
	status = inb(MOUSE_STATUS);
	if ((status & MOUSE_OBUF_FULL) != MOUSE_OBUF_FULL)
		return;

	/* handle data */
	if ((status & MOUSE_BBIT) && (status & MOUSE_FBIT)) {
		value = inb(MOUSE_PORT);

		switch (mouse_cycle) {
			case 0:
				/* get mouse state */
				mouse_byte[0] = value;
				if (value & MOUSE_VBIT)
					mouse_cycle++;
				break;
			case 1:
				/* get mouse x */
				mouse_byte[1] = value;
				mouse_cycle++;
				break;
			case 2:
				/* get mouse y */
				mouse_byte[2] = value;

				/* compute event */
				mouse_compute_event();
				break;
			default:
				break;
		}
	}
}

/*
 * Open a mouse buffer.
 */
static int mouse_open(struct file_t *filp)
{
	UNUSED(filp);
	return 0;
}

/*
 * Read a mouse.
 */
static int mouse_read(struct file_t *filp, char *buf, int n)
{
	/* unused filp */
	UNUSED(filp);

	/* user must read at least 3 characters = mouse event size */
	if (n < MOUSE_EVENT_SIZE)
		return -EINVAL;

	/* read only one event */
	return ring_buffer_read(&mouse_rb, (uint8_t *) buf, MOUSE_EVENT_SIZE);
}

/*
 * Writ to a mouse.
 */
static int mouse_write(struct file_t *filp, const char *buf, int n)
{
	int i;

	/* unused filp */
	UNUSED(filp);

	for (i = 0; i < n; i++)
		mouse_write_dev(buf[i]);

	return n;
}

/*
 * Poll a mouse.
 */
static int mouse_poll(struct file_t *filp, struct select_table_t *wait)
{
	int mask = POLLOUT;

	/* unused filp */
	UNUSED(filp);

	/* check if there is some characters to read */
	if (ring_buffer_left(&mouse_rb) > 0)
		mask |= POLLIN;

	/* add wait wait queue to select table */
	select_wait(&mouse_rb.wait, wait);

	return mask;
}

/*
 * Init mouse.
 */
int init_mouse()
{
	int ret;

	/* create mouse buffer */
	ret = ring_buffer_init(&mouse_rb, MOUSE_BUF_SIZE);
	if (ret)
		return ret;

	/* register interrupt handler */
	register_interrupt_handler(44, mouse_handler);

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
	mouse_poll_status_no_sleep();

	/* enable mouse */
	outb(MOUSE_COMMAND, MOUSE_ENABLE);
	mouse_write_dev(MOUSE_ENABLE_DEV);
	mouse_write_cmd(MOUSE_INTS_ON);
	mouse_poll_status_no_sleep();

	return 0;
}

/*
 * Mouse file operations.
 */
static struct file_operations_t mouse_fops = {
	.open		= mouse_open,
	.read		= mouse_read,
	.write		= mouse_write,
	.poll		= mouse_poll,
};

/*
 * Mouse inode operations.
 */
struct inode_operations_t mouse_iops = {
	.fops		= &mouse_fops,
};
