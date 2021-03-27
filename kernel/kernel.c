#include <kernel/screen.h>
#include <kernel/gdt.h>
#include <kernel/idt.h>

int kmain()
{
  /* init gdt */
  init_gdt();

  /* init idt */
  init_idt();

  /* clear screen */
  screen_clear();

  /* test interruptions */
  asm volatile("int $0x3");
  asm volatile("int $0x4");

  return 0;
}
