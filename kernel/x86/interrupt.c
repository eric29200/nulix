#include <x86/interrupt.h>
#include <x86/io.h>
#include <sys/syscall.h>
#include <proc/sched.h>
#include <kernel_stat.h>
#include <stdio.h>

#define NR_EXCEPTIONS		129
#define NR_IRQS			224

/*
 * x86 cpu exceptions messages
 */
static const char *exception_messages[] = {
	"Division By Zero Exception",
	"Debug Exception",
	"Non Maskable Interrupt Exception",
	"Breakpoint Exception",
	"Into Detected Overflow Exception",
	"Out of Bounds Exception",
	"Invalid Opcode Exception",
	"No Coprocessor Exception",
	"Double Fault Exception",
	"Coprocessor Segment Overrun Exception",
	"Bad TSS Exception",
	"Segment Not Present Exception",
	"Stack Fault Exception",
	"General Protection Fault Exception",
	"Page Fault Exception",
	"Unknown Interrupt Exception",
	"Coprocessor Fault Exception",
	"Alignment Check Exception (486+)",
	"Machine Check Exception (Pentium/586+)"
};

/* exception and irq handlers */
static isr_t exception_handlers[NR_EXCEPTIONS] = { 0 };
static isr_t irq_handlers[NR_IRQS] = { 0 };

/*
 * Register an exception handler.
 */
void register_exception_handler(uint32_t n, isr_t handler)
{
	exception_handlers[n] = handler;
}

/*
 * Register an interrupt handler.
 */
void register_interrupt_handler(uint32_t n, isr_t handler)
{
	irq_handlers[n] = handler;
}

/*
 * Unregister an interrupt handler.
 */
void unregister_interrupt_handler(uint32_t n)
{
	irq_handlers[n] = NULL;
}

/*
 * Exception handler.
 */
void exception_handler(struct registers *regs)
{
	/* update kernel statistics */
	kstat.interrupts++;

	/* handle exception */
	if (exception_handlers[regs->int_no]) {
		exception_handlers[regs->int_no](regs);
		return;
	}

	/* print exception and exit */
	printf("[Interrupt] code=%d, message=%s (process %d @ 0x%x)\n",
		regs->int_no,
		regs->int_no < 20 ? exception_messages[regs->int_no]: "",
		current_task->pid,
		regs->eip);
	do_exit(SIGSEGV);
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