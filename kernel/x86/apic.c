#include <x86/apic.h>
#include <x86/io.h>
#include <stdio.h>

/*
 * Calibrate APIC timer.
 */
static uint32_t calibrate_apic_timer()
{
	uint16_t last_count = 0xFFFF, count;
	uint8_t low, high;
	uint32_t divisor;

	/* set PIT */
	divisor = 1193182 / 100;
	outb(0x43, 0x34);
	outb(0x40, (uint8_t) divisor);
	outb(0x40, (uint8_t) (divisor >> 8));

	apic_write(APIC_TDCR, 0x3);
	apic_write(APIC_TMICT, 0xFFFFFFFF);

	/* calibrate */
	for (;;) {
		outb(0x43, 0x00);
		low = inb(0x40);
		high = inb(0x40);
		count = low | (high << 8);

		if (count > last_count)
			break;

		last_count = count;
	}

	apic_write(APIC_LVTT, 0x10000);

	return 0xFFFFFFFF - apic_read(APIC_TMCCT);
}

/*
 * Init APIC timer.
 */
static void init_apic_timer()
{
	uint32_t ticks_per_10ms, ticks_per_tick;

	/* calibrate timer */
	ticks_per_10ms = calibrate_apic_timer();
	if (ticks_per_10ms < 1000)
		ticks_per_10ms = 150000;

	/* compute ticks per tick */
	ticks_per_tick = ticks_per_10ms * 100 / HZ;
	if (ticks_per_tick == 0)
		ticks_per_tick = 1000;

	/* init APIC timer */
	apic_write(APIC_LVTT, 32 | 0x20000);
	apic_write(APIC_TDCR, 0x3);
	apic_write(APIC_TMICT, ticks_per_tick);
}

/*
 * Init APIC.
 */
void init_apic()
{
	uint32_t value;

	/* set Task Priority to 'accept all' */
	value = apic_read(APIC_TASKPRI);
 	value &= ~APIC_TPRI_MASK;
	apic_write(APIC_TASKPRI, value);

	/* enable apic */
	value = apic_read(APIC_SPIV);
	value |= (1 << 8);		/* enable APIC (bit==1) */
	value &= ~(1 << 9);		/* enable focus processor (bit==0) */
	value |= 0xFF;			/* set spurious IRQ vector to 0xff */
	apic_write(APIC_SPIV, value);

	/* setup virtual wire mode */
	apic_write(APIC_LVT0, 0x700);
	apic_write(APIC_LVT1, 0x400);

	/* clear error */
	apic_write(APIC_LVTERR, 0);
	apic_write(APIC_LVTERR, 0);

	/* send end of interrupt */
	apic_write(APIC_EOI, 0);

	/* init timer */
	init_apic_timer();
}