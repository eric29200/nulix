#include <x86/idt.h>
#include <x86/io.h>
#include <stddef.h>
#include <string.h>

static struct idt_entry idt_entries[256];
static struct idt_ptr idt_ptr;

extern void idt_flush(uint32_t);

/*
 * Set interrupt entry.
 */
static void idt_set_gate(uint32_t num, uint32_t base, uint16_t selector, uint8_t flags)
{
	idt_entries[num].base_low = base & 0xFFFF;
	idt_entries[num].base_high = (base >> 16) & 0xFFFF;
	idt_entries[num].selector = selector;
	idt_entries[num].zero = 0;
	idt_entries[num].flags = flags | 0x60;
}

/*
 * Init Interrupt Descriptor Table.
 */
void init_idt()
{
	idt_ptr.limit = sizeof(struct idt_entry) * 256 - 1;
	idt_ptr.base = (uint32_t) &idt_entries;

	/* memzero idt entries */
	memset(&idt_entries, 0, sizeof(struct idt_entry) * 256);

	/* remap irq table */
	outb(0x20, 0x11);
	outb(0xA0, 0x11);
	outb(0x21, 0x20);
	outb(0xA1, 0x28);
	outb(0x21, 0x04);
	outb(0xA1, 0x02);
	outb(0x21, 0x01);
	outb(0xA1, 0x01);
	outb(0x21, 0x0);
	outb(0xA1, 0x0);

	/* setup x86 exceptions */
	idt_set_gate(0, (uint32_t) exception0, 0x08, 0x8E);
	idt_set_gate(1, (uint32_t) exception1, 0x08, 0x8E);
	idt_set_gate(2, (uint32_t) exception2, 0x08, 0x8E);
	idt_set_gate(3, (uint32_t) exception3, 0x08, 0x8E);
	idt_set_gate(4, (uint32_t) exception4, 0x08, 0x8E);
	idt_set_gate(5, (uint32_t) exception5, 0x08, 0x8E);
	idt_set_gate(6, (uint32_t) exception6, 0x08, 0x8E);
	idt_set_gate(7, (uint32_t) exception7, 0x08, 0x8E);
	idt_set_gate(8, (uint32_t) exception8, 0x08, 0x8E);
	idt_set_gate(9, (uint32_t) exception9, 0x08, 0x8E);
	idt_set_gate(10, (uint32_t) exception10, 0x08, 0x8E);
	idt_set_gate(11, (uint32_t) exception11, 0x08, 0x8E);
	idt_set_gate(12, (uint32_t) exception12, 0x08, 0x8E);
	idt_set_gate(13, (uint32_t) exception13, 0x08, 0x8E);
	idt_set_gate(14, (uint32_t) exception14, 0x08, 0x8E);
	idt_set_gate(15, (uint32_t) exception15, 0x08, 0x8E);
	idt_set_gate(16, (uint32_t) exception16, 0x08, 0x8E);
	idt_set_gate(17, (uint32_t) exception17, 0x08, 0x8E);
	idt_set_gate(18, (uint32_t) exception18, 0x08, 0x8E);
	idt_set_gate(19, (uint32_t) exception19, 0x08, 0x8E);
	idt_set_gate(20, (uint32_t) exception20, 0x08, 0x8E);
	idt_set_gate(21, (uint32_t) exception21, 0x08, 0x8E);
	idt_set_gate(22, (uint32_t) exception22, 0x08, 0x8E);
	idt_set_gate(23, (uint32_t) exception23, 0x08, 0x8E);
	idt_set_gate(24, (uint32_t) exception24, 0x08, 0x8E);
	idt_set_gate(25, (uint32_t) exception25, 0x08, 0x8E);
	idt_set_gate(26, (uint32_t) exception26, 0x08, 0x8E);
	idt_set_gate(27, (uint32_t) exception27, 0x08, 0x8E);
	idt_set_gate(28, (uint32_t) exception28, 0x08, 0x8E);
	idt_set_gate(29, (uint32_t) exception29, 0x08, 0x8E);
	idt_set_gate(30, (uint32_t) exception30, 0x08, 0x8E);
	idt_set_gate(31, (uint32_t) exception31, 0x08, 0x8E);
	idt_set_gate(128, (uint32_t) exception128, 0x08, 0x8E);

	/* setup irqs */
	idt_set_gate(32, (uint32_t) irq0, 0x08, 0x8E);
	idt_set_gate(33, (uint32_t) irq1, 0x08, 0x8E);
	idt_set_gate(34, (uint32_t) irq2, 0x08, 0x8E);
	idt_set_gate(35, (uint32_t) irq3, 0x08, 0x8E);
	idt_set_gate(36, (uint32_t) irq4, 0x08, 0x8E);
	idt_set_gate(37, (uint32_t) irq5, 0x08, 0x8E);
	idt_set_gate(38, (uint32_t) irq6, 0x08, 0x8E);
	idt_set_gate(39, (uint32_t) irq7, 0x08, 0x8E);
	idt_set_gate(40, (uint32_t) irq8, 0x08, 0x8E);
	idt_set_gate(41, (uint32_t) irq9, 0x08, 0x8E);
	idt_set_gate(42, (uint32_t) irq10, 0x08, 0x8E);
	idt_set_gate(43, (uint32_t) irq11, 0x08, 0x8E);
	idt_set_gate(44, (uint32_t) irq12, 0x08, 0x8E);
	idt_set_gate(45, (uint32_t) irq13, 0x08, 0x8E);
	idt_set_gate(46, (uint32_t) irq14, 0x08, 0x8E);
	idt_set_gate(47, (uint32_t) irq15, 0x08, 0x8E);

	/* flush idt entries */
	idt_flush((uint32_t) &idt_ptr);
}
