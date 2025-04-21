#include <x86/gdt.h>
#include <x86/tss.h>
#include <x86/system.h>
#include <stddef.h>
#include <string.h>

static struct gdt_entry gdt_entries[6];
static struct gdt_ptr gdt_ptr;
static struct tss_entry tss_entry;

/*
 * Set tss stack.
 */
void tss_set_stack(uint32_t ss, uint32_t esp)
{
	tss_entry.ss0 = ss;
	tss_entry.esp0 = esp;
}

/*
 * Set a Global Descriptor Table entry.
 */
static void gdt_set_entry(uint32_t id, uint32_t base, uint32_t limit, int flags)
{
	gdt_entries[id].limit0 = ((limit) >>  0) & 0xFFFF;
	gdt_entries[id].limit1 = ((limit) >> 16) & 0x000F;
	gdt_entries[id].base0 = ((base)  >>  0) & 0xFFFF;
	gdt_entries[id].base1 = ((base)  >> 16) & 0x00FF;
	gdt_entries[id].base2 = ((base)  >> 24) & 0x00FF;
	gdt_entries[id].type = ((flags) >>  0) & 0x000F;
	gdt_entries[id].s = ((flags) >>  4) & 0x0001;
	gdt_entries[id].dpl = ((flags) >>  5) & 0x0003;
	gdt_entries[id].p = ((flags) >>  7) & 0x0001;
	gdt_entries[id].avl = ((flags) >> 12) & 0x0001;
	gdt_entries[id].l = ((flags) >> 13) & 0x0001;
	gdt_entries[id].d = ((flags) >> 14) & 0x0001;
	gdt_entries[id].g = ((flags) >> 15) & 0x0001;
}

/*
 * Init the Global Descriptor Table.
 */
void init_gdt()
{
	uint32_t tss_base;
	uint32_t tss_limit;

	/* set gdt */
	gdt_ptr.base = (uint32_t) &gdt_entries;
	gdt_ptr.limit = sizeof(gdt_entries) - 1;

	/* set tss */
	tss_base = (uint32_t) &tss_entry;
	tss_limit = tss_base + sizeof(struct tss_entry);

	/* create kernel and user code/data segments */
	gdt_set_entry(0, 0, 0, 0);
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
