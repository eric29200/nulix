#include <kernel/isr.h>
#include <libc/stdio.h>

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

/*
 * Interrupt service routine handler.
 */
void isr_handler(struct registers_t regs)
{
  printf("[Interrupt] code=%d", regs.int_no);
  if (regs.int_no < 20)
    printf(", message=%s", exception_messages[regs.int_no]);
  printf("\n");
}
