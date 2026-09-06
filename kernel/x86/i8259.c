#include <x86/i8259.h>
#include <x86/io.h>
#include <x86/idt.h>
#include <x86/interrupt.h>

extern irq_desc_t irq_desc[NR_IRQS];

/*
 * Init PIC.
 */
static void init_8259A()
{
	outb(0x21, 0xFF);	/* mask all of 8259A-1 */
	outb(0xA1, 0xFF);	/* mask all of 8259A-2 */
	outb(0x20, 0x11);	/* ICW1: select 8259A-1 init */
	outb(0x21, 0x20);	/* ICW2: 8259A-1 IR0-7 mapped to 0x20-0x27 */
	outb(0x21, 0x04);	/* 8259A-1 (the master) has a slave on IR2 */
	outb(0x21, 0x01);	/* master expects normal EOI */
	outb(0xA0, 0x11);	/* ICW1: select 8259A-2 init */
	outb(0xA1, 0x20 + 8);	/* ICW2: 8259A-2 IR0-7 mapped to 0x28-0x2f */
	outb(0xA1, 0x02);	/* 8259A-2 is a slave on master's IR2 */
	outb(0xA1, 0x01);
	outb(0x21, 0x00);	/* restore master IRQ mask */
	outb(0xA1, 0x00);	/* restore slave IRQ mask */
}

/*
 * Mask an ack an interrupt.
 */
static void mask_and_ack_8259A(uint32_t irq)
{
	/* send reset signal to slave PIC (if irq > 7) */
	if (irq > 7)
		outb(0xA0, 0x20);

	/* send reset signal to master PIC */
	outb(0x20, 0x20);
}

/*
 * PIC irq type.
 */
static struct hw_interrupt_type i8259A_irq_type = {
	.name		= "XT-PIC",
	.ack		= mask_and_ack_8259A,
};

/*
 * Init interrupts.
 */
void init_irq()
{
	int i;

	/* init pic */
	init_8259A();

	for (i = 0; i < NR_IRQS; i++)
		irq_desc[i].handler = &i8259A_irq_type;

	/* init idt */
	init_idt();
}