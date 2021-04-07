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

void init_gdt();

#endif
