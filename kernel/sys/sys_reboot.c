#include <sys/syscall.h>
#include <drivers/keyboard.h>
#include <x86/io.h>
#include <reboot.h>
#include <stderr.h>

/*
 * Restart CPU.
 */
static int do_restart()
{
	uint8_t out;

	/* disable interrupts */
	irq_disable();

	/* clear keyboard buffer */
	out = 0x02;
	while (out & 0x02)
		out = inb(KEYBOARD_STATUS);

	/* pluse CPU reset line */
	outb(KEYBOARD_STATUS, KEYBOARD_RESET);
	halt();

	return 0;
}

/*
 * Reboot system call.
 */
int sys_reboot(int magic1, int magic2, int cmd, void *arg)
{
	/* unused argument */
	UNUSED(arg);
	printf("reboot\n");

	/* check magic */
	if ((uint32_t) magic1 != LINUX_REBOOT_MAGIC1 || (
		magic2 != LINUX_REBOOT_MAGIC2
		&& magic2 != LINUX_REBOOT_MAGIC2A
		&& magic2 != LINUX_REBOOT_MAGIC2B
		&& magic2 != LINUX_REBOOT_MAGIC2C))
		return -EINVAL;

	switch (cmd) {
		case LINUX_REBOOT_CMD_RESTART:
		case LINUX_REBOOT_CMD_RESTART2:
		case LINUX_REBOOT_CMD_POWER_OFF:
		case LINUX_REBOOT_CMD_HALT:
			return do_restart();
		case LINUX_REBOOT_CMD_CAD_ON:
			break;
		case LINUX_REBOOT_CMD_CAD_OFF:
			break;
		default:
			return -EINVAL;
	}

	return 0;
}
