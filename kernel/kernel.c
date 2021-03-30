#include <kernel/gdt.h>
#include <kernel/idt.h>
#include <kernel/mm.h>
#include <grub/multiboot.h>
#include <drivers/screen.h>
#include <drivers/timer.h>
#include <lib/stdio.h>
#include <lib/string.h>

#define TIMER_HZ      50

extern uint32_t loader;
extern uint32_t kernel_end;
extern uint32_t kernel_start;

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

  /* init paging (start at end of static kernel code and finish at end of memory) */
  printf("[Kernel] Memory Paging Init\n");
  init_mem((uint32_t) &kernel_end, mboot->mem_upper * 1024);

  /* init timer at 50 Hz */
  printf("[Kernel] Timer Init at %dHz\n", TIMER_HZ);
  init_timer(TIMER_HZ);

  /* enable interrupts */
  printf("[Kernel] Enable interruptions \n");
  __asm__("sti");

  uint32_t size = 40 * 1024 * 1024;
  char *a = (char *) kmalloc(size);
  memset(a, 'a', size);

  for (uint32_t i = 0; i < size; i++) {
    if (a[i] != 'a') {
      printf("%d\n", i);
      break;
    }
  }

  return 0;
}
