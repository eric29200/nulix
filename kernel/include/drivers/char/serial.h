#ifndef _SERIAL_H_
#define _SERIAL_H_

#define BASE_BAUD		(1843200 / 16)

#define UART_RX			0	/* In:  Receive buffer (DLAB=0) */
#define UART_TX			0	/* Out: Transmit buffer (DLAB=0) */
#define UART_DLL		0	/* Out: Divisor Latch Low (DLAB=1) */
#define UART_DLM		1	/* Out: Divisor Latch High (DLAB=1) */
#define UART_IER		1	/* Out: Interrupt Enable Register */
#define UART_IIR		2	/* In:  Interrupt ID Register */
#define UART_FCR		2	/* Out: FIFO Control Register */
#define UART_EFR		2	/* I/O: Extended Features Register */
#define UART_LCR		3	/* Out: Line Control Register */
#define UART_MCR		4	/* Out: Modem Control Register */
#define UART_LSR		5	/* In:  Line Status Register */
#define UART_MSR		6	/* In:  Modem Status Register */
#define UART_SCR		7	/* I/O: Scratch Register */

/*
 * Serial line.
 */
struct serial_state {
	int		magic;
	int		baud_base;
	int		port;
};

void init_serial_console();

#endif
