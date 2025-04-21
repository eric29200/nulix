#ifndef _GDT_H_
#define _GDT_H_

#include <proc/task.h>

/* Flags for _DESC_S (non-system) descriptors */
#define _DESC_ACCESSED			0x0001
#define _DESC_DATA_WRITABLE		0x0002
#define _DESC_CODE_READABLE		0x0002
#define _DESC_DATA_EXPAND_DOWN		0x0004
#define _DESC_CODE_CONFORMING		0x0004
#define _DESC_CODE_EXECUTABLE		0x0008

/* Common flags */
#define _DESC_S				0x0010
#define _DESC_DPL(dpl)			((dpl) << 5)
#define _DESC_PRESENT			0x0080

#define _DESC_LONG_CODE			0x2000
#define _DESC_DB			0x4000
#define _DESC_GRANULARITY_4K		0x8000

/* System descriptors have a numeric "type" field instead of flags */
#define _DESC_SYSTEM(code)		(code)

#define _DESC_DATA			(_DESC_S | _DESC_PRESENT | _DESC_ACCESSED | _DESC_DATA_WRITABLE)
#define _DESC_CODE			(_DESC_S | _DESC_PRESENT | _DESC_ACCESSED | _DESC_CODE_READABLE | _DESC_CODE_EXECUTABLE)
#define DESC_DATA32			(_DESC_DATA | _DESC_GRANULARITY_4K | _DESC_DB)
#define DESC_CODE32			(_DESC_CODE | _DESC_GRANULARITY_4K | _DESC_DB)
#define DESC_TSS32			(_DESC_SYSTEM(9) | _DESC_PRESENT)
#define DESC_USER			(_DESC_DPL(3))

/* GDT entries */
#define GDT_ENTRY_KERNEL_CS		1
#define GDT_ENTRY_KERNEL_DS		2
#define GDT_ENTRY_USER_CS		3
#define GDT_ENTRY_USER_DS		4
#define GDT_ENTRY_TSS			5
#define GDT_ENTRY_TLS			6

/*
 * Global Descriptor Table entry.
 */
struct gdt_entry {
	uint16_t	limit0;
	uint16_t	base0;
	uint16_t	base1:8;
	uint16_t	type:4;
	uint16_t	s:1;
	uint16_t	dpl:2;
	uint16_t	p:1;
	uint16_t	limit1:4;
	uint16_t	avl:1;
	uint16_t	l:1;
	uint16_t	d:1;
	uint16_t	g:1;
	uint16_t	base2:8;
} __attribute__((packed));

/*
 * Global Descriptor Table entry pointer.
 */
struct gdt_ptr {
	uint16_t	limit;
	uint32_t	base;
} __attribute__((packed));

/*
 * Task State Segment.
 */
struct tss_entry {
	 uint32_t prev_tss;
	 uint32_t esp0;
	 uint32_t ss0;
	 uint32_t esp1;
	 uint32_t ss1;
	 uint32_t esp2;
	 uint32_t ss2;
	 uint32_t cr3;
	 uint32_t eip;
	 uint32_t eflags;
	 uint32_t eax;
	 uint32_t ecx;
	 uint32_t edx;
	 uint32_t ebx;
	 uint32_t esp;
	 uint32_t ebp;
	 uint32_t esi;
	 uint32_t edi;
	 uint32_t es;
	 uint32_t cs;
	 uint32_t ss;
	 uint32_t ds;
	 uint32_t fs;
	 uint32_t gs;
	 uint32_t ldt;
	 uint16_t trap;
	 uint16_t iomap_base;
} __attribute__((packed));

extern uint32_t kernel_stack;
extern void gdt_flush(uint32_t);
extern void tss_flush();

void init_gdt();
void load_tss(struct task *task);

#endif
