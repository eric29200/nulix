#include <kernel/isr.h>
#include <kernel/screen.h>

/*
 * Interrupt service routine handler.
 */
void isr_handler(struct registers_t regs)
{
  printf("received interrput %d\n", regs.int_no);
}
