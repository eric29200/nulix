#include <kernel/gdt.h>
#include <kernel/idt.h>
#include <kernel/mm.h>
#include <kernel/kfs.h>
#include <grub/multiboot.h>
#include <drivers/screen.h>
#include <drivers/timer.h>
#include <lib/stdio.h>
#include <lib/string.h>

#define TIMER_HZ      50

extern uint32_t loader;

/*
 * Main kos function.
 */
int kmain(unsigned long magic, multiboot_info_t *mboot)
{
  uint32_t initrd_start;
  uint32_t initrd_end;

  /* check multiboot */
  if (magic != MULTIBOOT_BOOTLOADER_MAGIC)
    return 0xD15EA5E;

  /* clear screen */
  screen_clear();

  /* print grub informations */
  printf("[Kernel] Loading at linear address = %x\n", loader);

  /* get initrd location */
  initrd_start = *((uint32_t *) mboot->mods_addr);
  initrd_end = *((uint32_t *) (mboot->mods_addr + 4));

  /* init gdt */
  printf("[Kernel] Global Descriptor Table Init\n");
  init_gdt();

  /* init idt */
  printf("[Kernel] Interrupt Descriptor Table Init\n");
  init_idt();

  /* init memory */
  printf("[Kernel] Memory Init\n");
  init_mem(initrd_end, mboot->mem_upper * 1024);

  /* init timer at 50 Hz */
  printf("[Kernel] Timer Init at %dHz\n", TIMER_HZ);
  init_timer(TIMER_HZ);

  /* enable interrupts */
  printf("[Kernel] Enable interruptions \n");
  __asm__("sti");

  test(initrd_start);

  return 0;
}
