#include <kernel/gdt.h>
#include <kernel/idt.h>
#include <kernel/mm.h>
#include <kernel/interrupt.h>
#include <kernel/task.h>
#include <grub/multiboot.h>
#include <drivers/screen.h>
#include <drivers/timer.h>
#include <drivers/rtc.h>
#include <stdio.h>
#include <string.h>

extern uint32_t loader;
extern uint32_t kernel_end;
extern uint32_t kernel_stack;

void test1()
{
  uint32_t i = 0;

  while (1)
    printf("1 - %d\n", i++);
}

void test2()
{
  uint32_t i = 0;

  while (1)
    printf("2 - %d\n", i++);
}

/*
 * Main kos function.
 */
int kmain(unsigned long magic, multiboot_info_t *mboot, uint32_t initial_stack)
{
  /* check multiboot */
  if (magic != MULTIBOOT_BOOTLOADER_MAGIC)
    return 0xD15EA5E;

  /* disable interrupts */
  irq_disable();

  /* clear screen */
  screen_clear();

  /* print grub informations */
  printf("[Kernel] Loading at linear address = %x\n", loader);
  kernel_stack = initial_stack;

  /* init gdt */
  printf("[Kernel] Global Descriptor Table Init\n");
  init_gdt();

  /* init idt */
  printf("[Kernel] Interrupt Descriptor Table Init\n");
  init_idt();

  /* init memory */
  printf("[Kernel] Memory Init\n");
  init_mem((uint32_t) &kernel_end, mboot->mem_upper * 1024);

  /* init processes */
  printf("[Kernel] Processes Init");

  /* init timer */
  printf("[Kernel] Timer Init\n");
  init_timer();

  /* init real time clock */
  printf("[Kernel] Real Time Clock Init\n");
  init_rtc();

  /* enable interrupts */
  printf("[Kernel] Enable interrupts\n");
  irq_enable();

  create_thread(test1);
  create_thread(test2);

  return 0;
}
