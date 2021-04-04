#include <kernel/interrupt.h>
#include <kernel/io.h>
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

/* number of current cli */
static volatile int nb_cli = 0;

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
void isr_handler(struct registers_t regs)
{
  isr_t handler;

  /* print a message */
  printf("[Interrupt] code=%d", regs.int_no);
  if (regs.int_no < 20)
    printf(", message=%s", exception_messages[regs.int_no]);
  printf("\n");

  /* handle interrupt */
  if (interrupt_handlers[regs.int_no] != 0) {
    handler = interrupt_handlers[regs.int_no];
    handler(regs);
  }
}

/*
 * IRQ service routine handler
 */
void irq_handler(struct registers_t regs)
{
  isr_t handler;

  /* send reset signal to slave PIC (if irq > 7) */
  if (regs.int_no >= 40)
    outb(0xA0, 0x20);

  /* send reset signal to master PIC */
  outb(0x20, 0x20);

  /* handle interrupt */
  if (interrupt_handlers[regs.int_no] != 0) {
    handler = interrupt_handlers[regs.int_no];
    handler(regs);
  }
}


/*
 * Disable interrupts.
 */
void interrupts_disable()
{
	uint32_t flags;

  /* check if interrupts are enabled */
	__asm__ volatile("pushf\n\t"
                   "pop %%eax\n\t"
	                 "movl %%eax, %0\n\t"
	                 : "=r"(flags)
	                 :
	                 : "%eax");

	/* disable interrupts */
  __asm__ volatile("cli");

  /* if interrupts were enable, 1st cli */
	if (flags & (1 << 9))
		nb_cli = 1;
	else
    nb_cli++;
}

/*
 * Enable interruptions.
 */
void interrupts_enable()
{
  nb_cli = 0;
  __asm__ volatile("sti");
}

/*
 * Restore interrupts.
 */
void interrupts_restore()
{
	if (nb_cli <= 1)
    interrupts_enable();
  else
    nb_cli--;
}
