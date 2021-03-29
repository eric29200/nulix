#include <kernel/gdt.h>
#include <kernel/idt.h>
#include <kernel/mm.h>
#include <grub/multiboot.h>
#include <drivers/screen.h>
#include <drivers/timer.h>
#include <lib/stdio.h>

#define TIMER_HZ      50

extern uint32_t loader;
extern uint32_t kernel_end;

int kmain(unsigned long magic, multiboot_info_t *mboot)
{
  /* check multiboot */
  if (magic != MULTIBOOT_BOOTLOADER_MAGIC)
    return 0xD15EA5E;

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

  /* init timer at 50 Hz */
  printf("[Kernel] Timer Init at %dHz\n", TIMER_HZ);
  init_timer(TIMER_HZ);

  /* init paging (start at end of static kernel code and finish at end of memory) */
  printf("[Kernel] Memory Paging Init\n");
  init_mem((uint32_t) &kernel_end, 0x100000 + mboot->mem_upper * 1024);

  /* enable interrupts */
  __asm__("sti");

  int i;
  for (i = 0; i < 30; i++) {
    char *a = kmalloc(1024 * 1024);
    kfree(a);
    //char *a = kmalloc(1024);
    //kfree(a);
  }

  printf("ok\n");

  return 0;
}
