#ifndef _GDT_H_
#define _GDT_H_

/*
 * Global Descriptor Table entry.
 */
struct gdt_entry_t {
  unsigned short limit_low;
  unsigned short base_low;
  unsigned char base_middle;
  unsigned char access;
  unsigned char granularity;
  unsigned char base_high;
} __attribute__((packed));

/*
 * Global Descriptor Table entry pointer.
 */
struct gdt_ptr_t {
  unsigned short limit;
  unsigned int base;
} __attribute__((packed));

void init_gdt();

#endif
