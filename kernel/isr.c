#include <kernel/isr.h>
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
 * Enable interruptions.
 */
void interrupts_enable()
{
  __asm__ volatile("sti");
}

/*
 * Disable interrupts.
 */
void interrupts_disable()
{
  __asm__ volatile("cli");
}
