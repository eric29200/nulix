#include <kernel/gdt.h>
#include <kernel/idt.h>
#include <kernel/mm_paging.h>
#include <grub/multiboot.h>
#include <drivers/screen.h>
#include <drivers/timer.h>
#include <libc/stdio.h>

extern uint32_t loader;

int kmain(unsigned long magic, multiboot_info_t *mboot)
{
  multiboot_memory_map_t *mmap;
  uint32_t total_mem;

  /* check multiboot */
  if (magic != MULTIBOOT_BOOTLOADER_MAGIC)
    return 0xD15EA5E;

  /* clear screen */
  screen_clear();

  /* get total memory available */
  mmap = (multiboot_memory_map_t *) mboot->mmap_addr;
  while (mmap < (multiboot_memory_map_t *) (mboot->mmap_addr + mboot->mmap_length)) {
    if (mmap->type == MULTIBOOT_MEMORY_AVAILABLE)
      total_mem += mmap->len;

    mmap = (multiboot_memory_map_t *) ((uint32_t) mmap + mmap->size + sizeof(uint32_t));
  }

  /* print grub informations */
  printf("Kernel loading linear address = %x\n", loader);
  printf("Grub magic = %x\n", magic);
  if (mboot->flags & (1 << 2))
    printf("Grub cmdline = %s\n", (const char *) mboot->cmdline);
  printf("Total Memory = %d kB\n", total_mem / 1024);

  /* init gdt */
  init_gdt();

  /* init idt */
  init_idt();

  /* init timer at 50 Hz */
  init_timer(50);

  /* init paging */
  init_paging(total_mem);

  /* enable interrupts */
  __asm__("sti");

  return 0;
}
