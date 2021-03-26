#include "isr.h"
#include "../drivers/screen.h"

/*
 * Interrupt service routine handler.
 */
void isr_handler(struct registers_t regs)
{
  screen_puts("received interrupt ");
  screen_puti(regs.int_no);
  screen_putc('\n');
}
