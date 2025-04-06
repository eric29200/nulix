#include <x86/interrupt.h>
#include <x86/io.h>
#include <sys/syscall.h>
#include <proc/sched.h>
#include <kernel_stat.h>
#include <stdio.h>

/*
 * x86 cpu exceptions messages
 */
const char* exception_messages[] = {
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

/* isr handlers */
isr_t interrupt_handlers[256];

/*
 * Register an interrupt handler.
 */
void register_interrupt_handler(uint8_t n, isr_t handler)
{
	interrupt_handlers[n] = handler;
}

/*
 * Interrupt service routine handler.
 */
void isr_handler(struct registers *regs)
{
	isr_t handler;

	/* update kernel statistics */
	kstat.interrupts++;

	/* IRQ : send ack to pic */
	if (regs->int_no >= 32) {
		/* send reset signal to slave PIC (if irq > 7) */
		if (regs->int_no >= 40)
			outb(0xA0, 0x20);

		/* send reset signal to master PIC */
		outb(0x20, 0x20);
	}

	/* handle interrupt or print a message */
	if (interrupt_handlers[regs->int_no] != 0) {
		handler = interrupt_handlers[regs->int_no];
		handler(regs);
	} else {
		printf("[Interrupt] code=%d, message=%s (process %d @ %x)\n",
		       regs->int_no,
		       regs->int_no < 20 ? exception_messages[regs->int_no]: "",
		       current_task->pid,
		       regs->eip);

		sys_exit(1);
	}

	/* schedule if needed */
	if (need_resched)
		schedule();
}
