#include <drivers/char/pit.h>
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
volatile time_t jiffies = 0;
struct kernel_timeval xtimes = { 0, 0 };

/* Time Stamp Counter variables */
static uint32_t tsc_quotient;
static uint32_t last_tsc_low;

/*
 * Get time offset since last interrupt.
 */
static inline uint32_t do_gettimeoffset(void)
{
	register uint32_t eax asm("ax");
	register uint32_t edx asm("dx");

	/* read the Time Stamp Counter */
	rdtsc(eax,edx);

	/* .. relative to previous jiffy (32 bits is enough) */
	eax -= last_tsc_low;

	/*
         * Time offset = (tsc_low delta) * fast_gettimeoffset_quotient
         *             = (tsc_low delta) * (usecs_per_clock)
         *             = (tsc_low delta) * (usecs_per_jiffy / clocks_per_jiffy)
         */
	__asm__("mull %2"
		:"=a" (eax), "=d" (edx)
		:"g" (tsc_quotient),
		 "0" (eax));

	/* time offset in microseconds */
	return edx;
}

/*
 * Calibrate Time Stamp Counter.
 */
static uint32_t calibrate_tsc()
{
	uint32_t start_low, start_high, end_low, end_high, count = 0;

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
static void pit_handler(struct registers *regs)
{
	uint32_t time_offset;

	/* unused register */
	UNUSED(regs);

	/* get time offset since last interrupt */
	time_offset = do_gettimeoffset();

	/* update xtimes */
	xtimes.tv_nsec += time_offset * 1000L;
	xtimes.tv_sec += xtimes.tv_nsec / 1000000000L;
	xtimes.tv_nsec %= 1000000000L;

	/* save last Time Stamp Counter */
	rdtscl(last_tsc_low);

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
	uint32_t cpu_khz, divisor, eax, edx;

	/* get CPU frequency */
	tsc_quotient = calibrate_tsc();
	if (tsc_quotient) {
		eax = 0;
		edx = 1000;
		__asm__("divl %2"
			:"=a" (cpu_khz), "=d" (edx)
			:"r" (tsc_quotient),
			"0" (eax), "1" (edx));
		printf("[Kernel] Detected %d.%d MHz processor.\n", cpu_khz / 1000, cpu_khz % 1000);
	}

	/* send command and frequency divisor */
	divisor = CLOCK_TICK_RATE / HZ;
	outb(0x43, 0x36);
	outb(0x40, divisor & 0xFF);
	outb(0x40, (divisor >> 8) & 0xFF);

	/* register irq */
	register_interrupt_handler(IRQ0, &pit_handler);
}
