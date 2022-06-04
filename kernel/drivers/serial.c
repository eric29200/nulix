#include <drivers/serial.h>
#include <x86/io.h>

/*
 * Wait for empty buffer.
 */
static int serial_transmit_empty()
{
	 return inb(PORT_COM1 + 5) & 0x20;
}

/*
 * Write to serial console.
 */
void write_serial(char c)
{
	 while (serial_transmit_empty() == 0);
	 outb(PORT_COM1, c);
}

/*
 * Init serial console.
 */
void init_serial()
{
	 outb(PORT_COM1 + 1, 0x00);	/* disable interrupts */
	 outb(PORT_COM1 + 3, 0x80);	/* set baud rate divisor */
	 outb(PORT_COM1 + 0, 0x03);	/* set divisor to 3 */
	 outb(PORT_COM1 + 1, 0x00);
	 outb(PORT_COM1 + 3, 0x03);	/* 8 bits, no parity and one stop bit */
	 outb(PORT_COM1 + 2, 0xC7);	/* enable FIFO mode */
	 outb(PORT_COM1 + 4, 0x0B);	/* enable interrupts */
}
