#include <kernel/interrupt.h>
#include <kernel/io.h>
#include <kernel/mm.h>
#include <kernel/system.h>
#include <drivers/timer.h>
#include <stdio.h>
#include <stderr.h>

/* current jiffies */
volatile uint32_t jiffies = 0;

/*
 * Timer handler.
 */
static void timer_handler(struct registers_t *regs)
{
  UNUSED(regs);

  /* update jiffies */
  jiffies++;
}

/*
 * Init the Programmable Interval Timer.
 */
void init_timer()
{
  uint32_t divisor;
  uint8_t divisor_low;
  uint8_t divisor_high;

  /* register irq */
  register_interrupt_handler(IRQ0, &timer_handler);

  /* set frequency (PIT's clock is always at 1193180 Hz) */
  divisor = 1193182 / HZ;
  divisor_low = (uint8_t) (divisor & 0xFF);
  divisor_high = (uint8_t) ((divisor >> 8) & 0xFF);

  /* send command and frequency divisor */
  outb(0x43, 0x36);
  outb(0x40, divisor_low);
  outb(0x40, divisor_high);
}
