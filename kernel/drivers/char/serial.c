#include <drivers/char/serial.h>
#include <lib/console.h>
#include <x86/io.h>

/*
 * Serial table.
 */
static struct serial_state rs_table[] = {
	{ 0, BASE_BAUD, 0x3F8 },
	{ 0, BASE_BAUD, 0x2F8 },
	{ 0, BASE_BAUD, 0x3E8 },
	{ 0, BASE_BAUD, 0x2E8 }
};

/*
 * Write to serial console.
 */
static void serial_console_write(struct console *co, const char *buf, size_t len)
{
	struct serial_state *ser = rs_table + co->index;
	size_t i;

	for (i = 0; i < len; i++, buf++) {
		/* wait for serial line */
	 	while (!(inb(ser->port + UART_LSR) & 0x20));

		/* put character */
	 	outb(ser->port + UART_TX, *buf);
	}
}

/*
 * Init serial console.
 */
static int serial_console_setup(struct console *co)
{
	struct serial_state *ser = rs_table + co->index;

	outb(ser->port + UART_IER, 0x00);	/* disable interrupts */
	outb(ser->port + UART_LCR, 0x80);	/* set baud rate divisor */
	outb(ser->port + UART_DLL, 0x03);	/* set divisor to 3 */
	outb(ser->port + UART_LCR, 0x03);	/* 8 bits, no parity and one stop bit */
	outb(ser->port + UART_FCR, 0xC7);	/* enable FIFO mode */

	return 0;
}

/*
 * Serial console.
 */
static struct console serial_console = {
	.name		= "ttyS",
	.index		= -1,
	.write		= serial_console_write,
	.setup		= serial_console_setup,
};

/*
 * Init serial console.
 */
void init_serial_console()
{
	return register_console(&serial_console);
}