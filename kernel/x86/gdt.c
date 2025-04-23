#include <x86/gdt.h>
#include <x86/system.h>
#include <proc/task.h>
#include <stddef.h>
#include <string.h>

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

/* Global Descriptor Table */
static struct desc_struct gdt[GDT_ENTRIES];
static struct gdt_ptr gdt_ptr;
static struct tss_entry tss_entry;

/*
 * Load Task State Segment.
 */
void load_tss(uint32_t esp0)
{
	tss_entry.esp0 = esp0;
}

/*
 * Set a Global Descriptor Table entry.
 */
static void gdt_set_entry(uint32_t id, uint32_t base, uint32_t limit, int flags)
{
	gdt[id].limit0 = ((limit) >>  0) & 0xFFFF;
	gdt[id].limit1 = ((limit) >> 16) & 0x000F;
	gdt[id].base0 = ((base)  >>  0) & 0xFFFF;
	gdt[id].base1 = ((base)  >> 16) & 0x00FF;
	gdt[id].base2 = ((base)  >> 24) & 0x00FF;
	gdt[id].type = ((flags) >>  0) & 0x000F;
	gdt[id].s = ((flags) >>  4) & 0x0001;
	gdt[id].dpl = ((flags) >>  5) & 0x0003;
	gdt[id].p = ((flags) >>  7) & 0x0001;
	gdt[id].avl = ((flags) >> 12) & 0x0001;
	gdt[id].l = ((flags) >> 13) & 0x0001;
	gdt[id].d = ((flags) >> 14) & 0x0001;
	gdt[id].g = ((flags) >> 15) & 0x0001;
}

/*
 * Write a Global Descriptor Table entry.
 */
void gdt_write_entry(size_t id, struct desc_struct *entry)
{
	memcpy(&gdt[id], entry, sizeof(struct desc_struct));
}

/*
 * Init the Global Descriptor Table.
 */
void init_gdt()
{
	uint32_t tss_base;
	uint32_t tss_limit;

	/* set gdt */
	gdt_ptr.base = (uint32_t) &gdt;
	gdt_ptr.limit = sizeof(gdt) - 1;

	/* set tss */
	tss_base = (uint32_t) &tss_entry;
	tss_limit = tss_base + sizeof(struct tss_entry);

	/* create kernel and user code/data segments */
	memset(gdt, 0, sizeof(struct desc_struct) * GDT_ENTRIES);
	gdt_set_entry(GDT_ENTRY_KERNEL_CS, 0, 0xFFFFF, DESC_CODE32);
	gdt_set_entry(GDT_ENTRY_KERNEL_DS, 0, 0xFFFFF, DESC_DATA32);
	gdt_set_entry(GDT_ENTRY_USER_CS, 0, 0xFFFFF, DESC_CODE32 | DESC_USER);
	gdt_set_entry(GDT_ENTRY_USER_DS, 0, 0xFFFFF, DESC_DATA32 | DESC_USER);
	gdt_set_entry(GDT_ENTRY_TSS, tss_base, tss_limit, DESC_TSS32);

	/* init tss */
	memset((void *) &tss_entry, 0, sizeof(struct tss_entry));
	tss_entry.ss0 = 0x10;
	tss_entry.esp0 = 0x0;
	tss_entry.cs = 0x0B;
	tss_entry.ss = 0x13;
	tss_entry.es = 0x13;
	tss_entry.ds = 0x13;
	tss_entry.fs = 0x13;
	tss_entry.gs = 0x13;
	tss_entry.iomap_base = sizeof(struct tss_entry);

	/* flush gdt */
	gdt_flush((uint32_t) &gdt_ptr);

	/* flush tss */
	tss_flush();
}
