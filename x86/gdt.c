#include <x86/gdt.h>
#include <x86/tss.h>
#include <x86/system.h>
#include <stddef.h>
#include <string.h>

extern uint32_t kernel_stack;
extern void gdt_flush(uint32_t);
extern void tss_flush();

struct gdt_entry_t gdt_entries[6];
struct gdt_ptr_t gdt_ptr;
struct tss_entry_t tss_entry;

/*
 * Set tss stack.
 */
void tss_set_stack(uint32_t ss, uint32_t esp)
{
  tss_entry.ss0 = ss;
  tss_entry.esp0 = esp;
}

/*
 * Set a Global Descriptor Table.
 */
static void gdt_set_gate(uint32_t num, uint32_t base, uint32_t limit, uint8_t access, uint8_t  gran)
{
  gdt_entries[num].base_low = base & 0xFFFF;
  gdt_entries[num].base_middle = (base >> 16) & 0xFF;
  gdt_entries[num].base_high = (base >> 24) & 0xFF;

  gdt_entries[num].limit_low = limit & 0xFFFF;
  gdt_entries[num].granularity = (limit >> 16) & 0x0F;

  gdt_entries[num].granularity |= gran & 0xF0;
  gdt_entries[num].access = access;
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
  tss_limit = tss_base + sizeof(struct tss_entry_t);

  /* create kernel and user code/data segments */
  gdt_set_gate(0, 0, 0, 0, 0);                        /* NULL segement : always needed */
  gdt_set_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xCF);         /* Kernel Code segment */
  gdt_set_gate(2, 0, 0xFFFFFFFF, 0x92, 0xCF);         /* Kernel Data segment */
  gdt_set_gate(3, 0, 0xFFFFFFFF, 0xFA, 0xCF);         /* User mode code segment */
  gdt_set_gate(4, 0, 0xFFFFFFFF, 0xF2, 0xCF);         /* User mode data segement */
  gdt_set_gate(5, tss_base, tss_limit, 0xE9, 0x00);   /* Task State Segment */

  /* init tss */
  memset((void *) &tss_entry, 0, sizeof(struct tss_entry_t));
  tss_entry.ss0 = 0x10;
  tss_entry.esp0 = 0x0;
  tss_entry.cs = 0x0B;
  tss_entry.ss = 0x13;
  tss_entry.es = 0x13;
  tss_entry.ds = 0x13;
  tss_entry.fs = 0x13;
  tss_entry.gs = 0x13;

  /* flush gdt */
  gdt_flush((uint32_t) &gdt_ptr);

  /* flush tss */
  tss_flush();
}
