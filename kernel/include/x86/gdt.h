#ifndef _GDT_H_
#define _GDT_H_

#include <stddef.h>

/*
 * Global Descriptor Table entry.
 */
struct gdt_entry_t {
	uint16_t	limit_low;
	uint16_t	base_low;
	uint8_t		base_middle;
	uint8_t		access;
	uint8_t		granularity;
	uint8_t		base_high;
} __attribute__((packed));

/*
 * Global Descriptor Table entry pointer.
 */
struct gdt_ptr_t {
	uint16_t	limit;
	uint32_t	base;
} __attribute__((packed));

void init_gdt();

#endif
