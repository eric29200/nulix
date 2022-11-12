#include <drivers/pit.h>
#include <x86/interrupt.h>
#include <x86/io.h>
#include <x86/system.h>
#include <mm/mm.h>
#include <proc/sched.h>
#include <time.h>
#include <stdio.h>
#include <stderr.h>

#define CLOCK_TICK_RATE		1193180
#define LATCH			((CLOCK_TICK_RATE + HZ / 2) / HZ)
#define CALIBRATE_LATCH		(5 * LATCH)
#define CALIBRATE_TIME		(5 * 1000020 / HZ)

/* current jiffies */
volatile uint32_t jiffies = 0;

/*
 * Get CPU frequency.
 */
static uint32_t get_cpu_frequency()
{
	uint32_t eax, edx, cpu_khz, start_low, start_high, end_low, end_high, count = 0;

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

	/* get CPU frequency in khz */
	eax = 0;
	edx = 1000;
	__asm__("divl %2"
		:"=a" (cpu_khz), "=d" (edx)
		:"r" (end_low),
		"0" (eax), "1" (edx));

	return cpu_khz;
}

/*
 * PIT handler.
 */
static void pit_handler(struct registers_t *regs)
{
	UNUSED(regs);

	/* update jiffies */
	jiffies++;

	/* schedule tasks */
	schedule();
}

/*
 * Init the Programmable Interval Timer.
 */
void init_pit()
{
	uint32_t cpu_khz, divisor;

	/* get CPU frequency */
	cpu_khz = get_cpu_frequency();
	printf("[Kernel] Detected %d.%d MHz processor.\n", cpu_khz / 1000, cpu_khz % 1000);

	/* send command and frequency divisor */
	divisor = CLOCK_TICK_RATE / HZ;
	outb(0x43, 0x36);
	outb(0x40, divisor & 0xFF);
	outb(0x40, (divisor >> 8) & 0xFF);

	/* register irq */
	register_interrupt_handler(IRQ0, &pit_handler);
}
