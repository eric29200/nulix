#ifndef _GDT_H_
#define _GDT_H_

#include <stddef.h>

/*
 * Global Descriptor Table entry.
 */
struct gdt_entry_t {
  uint16_t limit_low;
  uint16_t base_low;
  uint8_t base_middle;
  uint8_t seg_type : 4;
  uint8_t desc_type : 1;
  uint8_t dpl : 2;
  uint8_t present : 1;
  uint8_t limit_high : 4;
  uint8_t avl : 1;
  uint8_t op64 : 1;
  uint8_t opsize : 1;
  uint8_t granularity :1;
  uint8_t base_high;
} __attribute__((packed));

/*
 * Global Descriptor Table entry pointer.
 */
struct gdt_ptr_t {
  uint16_t limit;
  uint32_t base;
} __attribute__((packed));

/*
 * Task State Segment entry.
 */
struct tss_entry_t {
  unsigned short prevtask, r_prevtask;
  unsigned int esp0;
  unsigned short ss0, r_ss0;
  unsigned int esp1;
  unsigned short ss1, r_ss1;
  unsigned int esp2;
  unsigned short ss2, r_ss2;
  unsigned int cr3;
  unsigned int eip;
  unsigned int eflags;
  unsigned int eax;
  unsigned int ecx;
  unsigned int edx;
  unsigned int ebx;
  unsigned int esp;
  unsigned int ebp;
  unsigned int esi;
  unsigned int edi;
  unsigned short es, r_es;
  unsigned short cs, r_cs;
  unsigned short ss, r_ss;
  unsigned short ds, r_ds;
  unsigned short fs, r_fs;
  unsigned short gs, r_gs;
  unsigned short ldt, r_ldt;
  unsigned short r_iombase, iombase;
} __attribute__ ((packed));

void init_gdt();

#endif
