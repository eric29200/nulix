#include <kernel/screen.h>
#include <kernel/gdt.h>
#include <kernel/idt.h>
#include <kernel/timer.h>

int kmain()
{
  /* clear screen */
  screen_clear();

  /* init gdt */
  init_gdt();

  /* init idt */
  init_idt();

  /* init timer at 50 Hz */
  init_timer(50);

  /* enable interrupts */
  __asm__("sti");

  return 0;
}
