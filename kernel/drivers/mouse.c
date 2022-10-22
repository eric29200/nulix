#include <drivers/mouse.h>
#include <x86/interrupt.h>
#include <proc/sched.h>
#include <x86/io.h>
#include <fs/fs.h>

#define MOUSE_PORT		0x60
#define MOUSE_STATUS		0x64

#define MOUSE_BBIT		0x01
#define MOUSE_FBIT		0x20
#define MOUSE_VBIT		0x08

#define BUTTON_LEFT		0x01
#define BUTTON_RIGHT		0x02
#define BUTTON_MIDDLE		0x04

/* mouse event */
static struct mouse_event_t mouse_event;
static uint8_t mouse_byte[4];
static int mouse_cycle = 0;
static struct wait_queue_t *mouse_wq = NULL;

/*
 * Wait for mouse device.
 */
static void mouse_wait(int type)
{
	if (type)
		while ((inb(MOUSE_STATUS) & 2) != 0);
	else
		while ((inb(MOUSE_STATUS) & 1) != 1);
}

/*
 * Output a value a to mouse device.
 */
static void mouse_output(uint8_t value)
{
	mouse_wait(1);
	outb(MOUSE_STATUS, 0xD4);
	mouse_wait(1);
	outb(MOUSE_PORT, value);
}

/*
 * Read a value from mouse device.
 */
static uint8_t mouse_input()
{
	mouse_wait(0);
	return inb(MOUSE_PORT);
}

/*
 * Update mouse position.
 */
static void mouse_update_position()
{
	int move_x, move_y;
	uint8_t state;

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

	/* set mouse event */
	mouse_event.x = move_x;
	mouse_event.y = move_y;
	mouse_event.state = mouse_event.buttons;

	/* set mouse event button */
	if (state & BUTTON_LEFT)
		mouse_event.buttons |= BUTTON_LEFT;
	if (state & BUTTON_RIGHT)
		mouse_event.buttons |= BUTTON_RIGHT;
	if (state & BUTTON_MIDDLE)
		mouse_event.buttons |= BUTTON_MIDDLE;
}

/*
 * Mouse interrupt handler.
 */
static void mouse_handler(struct registers_t *regs)
{
	uint8_t status, value;

	UNUSED(regs);

	/* get mouse status */
	status = inb(MOUSE_STATUS);

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

				/* update position */
				mouse_update_position();
				break;
			default:
				break;
		}
	}

	/* wake up readers */
	task_wakeup_all(&mouse_wq);
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
	UNUSED(filp);

	/* copy mouse event */
	if (n >= (int) sizeof(struct mouse_event_t))
		memcpy(buf, &mouse_event, sizeof(struct mouse_event_t));

	/* reset mouse event */
	memset(&mouse_event, 0, sizeof(struct mouse_event_t));

	return 0;
}

/*
 * Poll a mouse.
 */
static int mouse_poll(struct file_t *filp, struct select_table_t *wait)
{
	int mask = 0;

	UNUSED(filp);

	/* check if an event is queued */
	if (mouse_event.x || mouse_event.y || mouse_event.buttons || mouse_event.state)
		mask |= POLLIN;

	/* add wait queue to select table */
	select_wait(&mouse_wq, wait);

	return mask;
}

/*
 * Init mouse.
 */
void init_mouse()
{
	uint8_t status;

	/* register interrupt handler */
	register_interrupt_handler(44, mouse_handler);

	/* init mouse event */
	mouse_event.x = 0;
	mouse_event.y = 0;
	mouse_event.buttons = 0;
	mouse_event.state = 0;

	/* empty mouse input buffer */
	while (inb(MOUSE_STATUS) & 0x01)
		inb(MOUSE_PORT);

	/* activate mouse */
	mouse_wait(1);
	outb(MOUSE_STATUS, 0xA8);
	mouse_wait(1);
	outb(MOUSE_STATUS, 0x20);
	mouse_wait(0);
	status = inb(MOUSE_PORT) | 3;
	mouse_wait(1);
	outb(MOUSE_STATUS, 0x60);
	mouse_wait(1);
	outb(MOUSE_PORT, status);

	/* set mouse rate */
	mouse_output(0xF6);
	mouse_input();

	/* start mouse */
	mouse_output(0xF4);
	mouse_input();
}

/*
 * Mouse file operations.
 */
static struct file_operations_t mouse_fops = {
	.open		= mouse_open,
	.read		= mouse_read,
	.poll		= mouse_poll,
};

/*
 * Mouse inode operations.
 */
struct inode_operations_t mouse_iops = {
	.fops		= &mouse_fops,
};
