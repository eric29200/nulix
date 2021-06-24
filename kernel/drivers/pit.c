#include <x86/interrupt.h>
#include <x86/io.h>
#include <x86/system.h>
#include <mm/mm.h>
#include <proc/sched.h>
#include <drivers/pit.h>
#include <time.h>
#include <stdio.h>
#include <stderr.h>

#define CLOCK_TICK_RATE     1193180
#define LATCH               ((CLOCK_TICK_RATE + HZ / 2) / HZ)
#define CALIBRATE_LATCH     (5 * LATCH)
#define CALIBRATE_TIME      (5 * 1000020 / HZ)

/* current jiffies */
volatile uint32_t jiffies = 0;
volatile uint32_t time_offset_us = 0;

/* time stamp counter */
uint32_t last_tsc_low = 0;
uint32_t tsc_quotient = 0;

/*
 * Calibrate TimeStamp Counter.
 */
static uint32_t calibrate_tsc()
{
  uint32_t start_low, start_high;
  uint32_t end_low, end_high;
  uint32_t count;

  /* set the gate high, disable speaker */
  outb(0x61, (inb(0x61) & ~0x02) | 0x01);

  /* set PIT */
  outb(0x43, 0xB0);
  outb(0x42, CALIBRATE_LATCH & 0xFF);
  outb(0x42, CALIBRATE_LATCH >> 8);

  /* get start count down */
  rdtsc(start_low, start_high);

  /* calibrate */
  do {
    count++;
  } while((inb(0x61) & 0x20) == 0);

  /* get end count down */
  rdtsc(end_low, end_high);
  last_tsc_low = end_low;

  if (count <= 1)
    return 0;

  /* 64 bit substract */
  __asm__("subl %2,%0\n\t"
          "sbbl %3,%1"
          :"=a" (end_low), "=d" (end_high)
          :"g" (start_low), "g" (start_high),
          "0" (end_low), "1" (end_high));

  /* cpu too fast */
  if (end_high)
    return 0;

  /* cpu too slow */
  if (end_low <= CALIBRATE_TIME)
    return 0;

  /* get 2^32 * (1 / x)) */
  __asm__("divl %2"
          :"=a" (end_low), "=d" (end_high)
          :"r" (end_low), "0" (0), "1" (CALIBRATE_TIME));

  return end_low;
}

/*
 * PIT handler.
 */
static void pit_handler(struct registers_t *regs)
{
  UNUSED(regs);

  /* update jiffies */
  jiffies++;

  /* get time offset in micro second */
  time_offset_us = get_time_offset();

  /* schedule tasks */
  schedule();
}

/*
 * Init the Programmable Interval Timer.
 */
void init_pit()
{
  uint32_t eax, edx, cpu_khz, divisor;
  uint8_t divisor_low, divisor_high;

  /* calibrate time stamp counter */
  tsc_quotient = calibrate_tsc();

  /* get CPU frequency */
  eax = 0, edx = 1000;
  __asm__("divl %2"
          :"=a" (cpu_khz), "=d" (edx)
          :"r" (tsc_quotient),
          "0" (eax), "1" (edx));

  /* print processor frequency */
  printf("[Kernel] Detected %d.%d MHz processor.\n", cpu_khz / 1000, cpu_khz % 1000);

  /* register irq */
  register_interrupt_handler(IRQ0, &pit_handler);

  /* set frequency (PIT's clock is always at 1193180 Hz) */
  divisor = 1193182 / HZ;
  divisor_low = (uint8_t) (divisor & 0xFF);
  divisor_high = (uint8_t) ((divisor >> 8) & 0xFF);

  /* send command and frequency divisor */
  outb(0x43, 0x36);
  outb(0x40, divisor_low);
  outb(0x40, divisor_high);
}
