#include <kernel/timer.h>
#include <kernel/isr.h>
#include <kernel/io.h>
#include <libc/stdio.h>

static volatile uint32_t tick = 1;

/*
 * Timer handler.
 */
static void timer_handler(struct registers_t regs)
{
  UNUSED(regs);
  printf("Tick : %d\n", ++tick);
}

/*
 * Init the Programmable Interval Timer.
 */
void init_timer(uint32_t frequency)
{
  uint32_t divisor;
  uint8_t divisor_low;
  uint8_t divisor_high;

  /* register irq */
  register_interrupt_handler(IRQ0, &timer_handler);

  /* set frequency (PIT's clock is always at 1193180 Hz) */
  divisor = 1193182 / frequency;
  divisor_low = (uint8_t) (divisor & 0xFF);
  divisor_high = (uint8_t) ((divisor >> 8) & 0xFF);

  /* send command and frequency divisor */
  outb(0x43, 0x36);
  outb(0x40, divisor_low);
  outb(0x40, divisor_high);
}
