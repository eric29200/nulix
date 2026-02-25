#include <x86/interrupt.h>
#include <x86/io.h>
#include <sys/syscall.h>
#include <proc/sched.h>
#include <kernel_stat.h>
#include <stdio.h>

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

/* exception handlers */
typedef void (*handler_t)(struct registers *);
static handler_t exception_handlers[NR_EXCEPTIONS] = { 0 };

/*
 * Register an exception handler.
 */
void register_exception_handler(uint32_t n, void *handler)
{
	exception_handlers[n] = handler;
}

/*
 * Exception handler.
 */
void exception_handler(struct registers *regs)
{
	/* handle exception */
	if (exception_handlers[regs->int_no]) {
		exception_handlers[regs->int_no](regs);
		return;
	}

	/* print exception and exit */
	printf("[Interrupt] code=%d, message=%s (process %d - %s @ 0x%x)\n",
		regs->int_no,
		regs->int_no < 20 ? exception_messages[regs->int_no]: "",
		current_task->pid,
		current_task->name,
		regs->eip);
	do_exit(SIGSEGV);
}