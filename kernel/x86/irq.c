#include <x86/interrupt.h>
#include <x86/io.h>
#include <sys/syscall.h>
#include <proc/sched.h>
#include <kernel_stat.h>
#include <stdio.h>

#define NR_IRQS			224

/* irq handlers */
typedef void (*handler_t)(struct registers *);
static handler_t irq_handlers[NR_IRQS] = { 0 };

/*
 * Request an IRQ.
 */
void request_irq(uint32_t n, void *handler)
{
	irq_handlers[n] = handler;
}

/*
 * Free an IRQ.
 */
void free_irq(uint32_t n)
{
	irq_handlers[n] = NULL;
}

/*
 * IRQ handler.
 */
void irq_handler(struct registers *regs)
{
	/* update kernel statistics */
	kstat.interrupts++;

	/* send reset signal to slave PIC (if irq > 7) */
	if (regs->int_no > 7)
		outb(0xA0, 0x20);

	/* send reset signal to master PIC */
	outb(0x20, 0x20);

	/* handle interrupt */
	if (irq_handlers[regs->int_no])
		irq_handlers[regs->int_no](regs);
}