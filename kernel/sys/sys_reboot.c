#include <sys/syscall.h>
#include <drivers/keyboard.h>
#include <x86/io.h>

/*
 * Reboot system call.
 */
int sys_reboot()
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
