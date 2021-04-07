#include <x86/gdt.h>
#include <x86/system.h>
#include <stddef.h>
#include <string.h>

#define OPSIZE_16BIT          0
#define OPSIZE_32BIT          1

#define GRANULARITY_1B        0
#define GRANULARITY_4KB       1

#define DESC_SYSTEM           0
#define DESC_CODEDATA         1

#define SEG_READ_ONLY         0
#define SEG_READ_WRITE        2
#define SEG_EXECUTE_ONLY      8
#define SEG_EXECUTE_READ      10
#define SEG_TSS               9

extern uint32_t kernel_stack;
extern void gdt_flush(uint32_t);

struct gdt_entry_t gdt_entries[6];
struct gdt_ptr_t gdt_ptr;

/*
 * Set a Global Descriptor Table.
 */
static void gdt_set_gate(uint32_t num, uint32_t base, uint32_t limit, uint8_t dpl, uint8_t desc_type, uint8_t seg_type)
{
  gdt_entries[num].base_low = base & 0xFFFF;
  gdt_entries[num].base_middle = (base >> 16) & 0xFF;
  gdt_entries[num].base_high = (base >> 24) & 0xFF;

  gdt_entries[num].limit_low = limit & 0xFFFF;
  gdt_entries[num].limit_high = (limit >> 16) & 0x0F;

  gdt_entries[num].present = 1;
  gdt_entries[num].dpl = dpl;
  gdt_entries[num].desc_type = desc_type;
  gdt_entries[num].seg_type = seg_type;

  gdt_entries[num].granularity = GRANULARITY_4KB;
  gdt_entries[num].opsize = OPSIZE_32BIT;
  gdt_entries[num].op64 = 0;
  gdt_entries[num].avl = 0;

  if (num == 0) {
    gdt_entries[num].present = 0;
    gdt_entries[num].granularity = 0;
    gdt_entries[num].opsize = 0;
  }
}

/*
 * Init the Global Descriptor Table.
 */
void init_gdt()
{
  gdt_ptr.base = (uint32_t) &gdt_entries;
  gdt_ptr.limit = sizeof(gdt_entries) - 1;

  /* Create kernel and user code/data segments */
  gdt_set_gate(0, 0, 0, 0, 0, 0);                                        /* NULL segement : always needed */
  gdt_set_gate(1, 0, 0xFFFFFFFF, 0, DESC_CODEDATA, SEG_EXECUTE_READ);    /* Kernel Code segment */
  gdt_set_gate(2, 0, 0xFFFFFFFF, 0, DESC_CODEDATA, SEG_READ_WRITE);      /* Kernel Data segment */
  gdt_set_gate(3, 0, 0xFFFFFFFF, 3, DESC_CODEDATA, SEG_EXECUTE_READ);    /* User mode code segment = ring 3 */
  gdt_set_gate(4, 0, 0xFFFFFFFF, 3, DESC_CODEDATA, SEG_READ_WRITE);      /* User mode data segement = ring 3 */

  /* flush gdt */
  gdt_flush((uint32_t) &gdt_ptr);
}
