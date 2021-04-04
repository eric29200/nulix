#include <kernel/gdt.h>
#include <kernel/idt.h>
#include <kernel/mm.h>
#include <kernel/isr.h>
#include <grub/multiboot.h>
#include <drivers/screen.h>
#include <drivers/timer.h>
#include <lib/stdio.h>
#include <lib/string.h>

extern uint32_t loader;
extern uint32_t kernel_end;

/*
 * Main kos function.
 */
int kmain(unsigned long magic, multiboot_info_t *mboot)
{
  /* check multiboot */
  if (magic != MULTIBOOT_BOOTLOADER_MAGIC)
    return 0xD15EA5E;

  /* disable interrupts */
  interrupts_disable();

  /* clear screen */
  screen_clear();

  /* print grub informations */
  printf("[Kernel] Loading at linear address = %x\n", loader);

  /* init gdt */
  printf("[Kernel] Global Descriptor Table Init\n");
  init_gdt();

  /* init idt */
  printf("[Kernel] Interrupt Descriptor Table Init\n");
  init_idt();

  /* init memory */
  printf("[Kernel] Memory Init\n");
  init_mem((uint32_t) &kernel_end, mboot->mem_upper * 1024);

  /* init timer at 50 Hz */
  printf("[Kernel] Timer Init\n");
  init_timer();

  /* enable interrupts */
  printf("[Kernel] Enable interrupts\n");
  interrupts_enable();

  return 0;
}
