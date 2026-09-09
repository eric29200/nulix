#include <x86/io_apic.h>
#include <x86/apic.h>
#include <x86/io.h>
#include <stdio.h>
#include <string.h>

/* global variables */
extern irq_desc_t irq_desc[NR_IRQS];
static int nr_ioapic_registers[MAX_IO_APICS];

/*
 * Ack I/O APIC irq.
 */
static void ack_edge_ioapic_irq(uint32_t irq)
{
	UNUSED(irq);
	apic_write(APIC_EOI, 0);
}

/*
 * I/O APIC interrupt.
 */
static struct hw_interrupt_type ioapic_edge_irq_type = {
	"IO-APIC-edge",
	ack_edge_ioapic_irq,
};

/*
 * Setup IRQs.
 */
static void setup_IO_APIC_irqs()
{
	int apic, pin, irq, i;

	for (apic = 0; apic < nr_ioapics; apic++) {
		for (pin = 0; pin < nr_ioapic_registers[apic]; pin++) {
			/* find irq */
			i = irq = 0;
			while (i < apic)
				irq += nr_ioapic_registers[i++];
			irq += pin;

			/* install APIC irq on processor 0 */
			io_apic_write(apic, 0x10 + 2 * pin, 32 + irq);
			io_apic_write(apic, 0x11 + 2 * pin, 0);
			irq_desc[pin].handler = &ioapic_edge_irq_type;

			/* disable PIC on irq */
			if (irq < 16)
				disable_8259A_irq(irq);
		}
	}
}

/*
 * Init I/O APIC.
 */
void init_io_apic()
{
	/* set up irqs */
	setup_IO_APIC_irqs();
}