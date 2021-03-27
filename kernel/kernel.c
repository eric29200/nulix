#include "../include/screen.h"
#include "../include/gdt.h"
#include "../include/idt.h"

int main(struct multiboot *mboot_ptr)
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
