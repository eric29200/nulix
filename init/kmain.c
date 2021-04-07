#include <x86/gdt.h>
#include <x86/idt.h>
#include <x86/interrupt.h>
#include <mm/mm.h>
#include <proc/task.h>
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
  printf("1\n");
}

void test2()
{
  printf("2\n");
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
  if (init_task() != 0)
    panic("Cannot init processes\n");

  /* init timer */
  printf("[Kernel] Timer Init\n");
  init_timer();

  /* init real time clock */
  printf("[Kernel] Real Time Clock Init\n");
  init_rtc();

  /* enable interrupts */
  printf("[Kernel] Enable interrupts\n");
  irq_enable();

  /* start 2 threads */
  start_thread(test1);
  start_thread(test2);

  return 0;
}
