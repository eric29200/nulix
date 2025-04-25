#ifndef _IDT_H_
#define _IDT_H_

#include <stddef.h>

/*
 * Interrupt Descriptor Table entry.
 */
struct idt_entry {
	uint16_t	base_low;
	uint16_t	selector;
	uint8_t		zero;
	uint8_t		flags;
	uint16_t	base_high;
} __attribute__((packed));

/*
 * Interrupt Descriptor Table entry pointer.
 */
struct idt_ptr {
	uint16_t	limit;
	uint32_t	base;
} __attribute__((packed));

void init_idt();

/* isr routines (defined in interrupt.s) */
extern void isr0();
extern void isr1();
extern void isr2();
extern void isr3();
extern void isr4();
extern void isr5();
extern void isr6();
extern void isr7();
extern void isr8();
extern void isr9();
extern void isr10();
extern void isr11();
extern void isr12();
extern void isr13();
extern void isr14();
extern void isr15();
extern void isr16();
extern void isr17();
extern void isr18();
extern void isr19();
extern void isr20();
extern void isr21();
extern void isr22();
extern void isr23();
extern void isr24();
extern void isr25();
extern void isr26();
extern void isr27();
extern void isr28();
extern void isr29();
extern void isr30();
extern void isr31();
extern void isr32();
extern void isr33();
extern void isr34();
extern void isr35();
extern void isr36();
extern void isr37();
extern void isr38();
extern void isr39();
extern void isr40();
extern void isr41();
extern void isr42();
extern void isr43();
extern void isr44();
extern void isr45();
extern void isr46();
extern void isr47();
extern void isr128();

#endif
