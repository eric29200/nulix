#include <kernel/gdt.h>
#include <kernel/idt.h>
#include <mm/paging.h>
#include <grub/multiboot.h>
#include <drivers/screen.h>
#include <drivers/timer.h>
#include <lib/stdio.h>

extern uint32_t loader;

int kmain(unsigned long magic, multiboot_info_t *mboot)
{
  multiboot_memory_map_t *mmap;

  /* check multiboot */
  if (magic != MULTIBOOT_BOOTLOADER_MAGIC)
    return 0xD15EA5E;

  /* clear screen */
  screen_clear();

  /* print grub informations */
  printf("Kernel loading linear address = %x\n", loader);

  /* init gdt */
  init_gdt();

  /* init idt */
  init_idt();

  /* init timer at 50 Hz */
  init_timer(50);

  /* init paging */
  init_paging(0x300001, 0x100000 + mboot->mem_upper * 1024);

  /* enable interrupts */
  __asm__("sti");

  return 0;
}
