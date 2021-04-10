#include <x86/gdt.h>
#include <x86/idt.h>
#include <x86/interrupt.h>
#include <mm/mm.h>
#include <proc/sched.h>
#include <proc/wait.h>
#include <grub/multiboot.h>
#include <drivers/serial.h>
#include <drivers/pit.h>
#include <drivers/rtc.h>
#include <semaphore.h>
#include <stdio.h>
#include <string.h>
#include <delay.h>

extern uint32_t loader;
extern uint32_t kernel_stack;

/*
 * Initialisation task.
 */
static void kinit(void *arg)
{
  UNUSED(arg);
}

/*
 * Main kos function.
 */
int kmain(unsigned long magic, multiboot_info_t *mboot, uint32_t initial_stack)
{
  uint32_t initrd_start;
  uint32_t initrd_end;

  /* check multiboot */
  if (magic != MULTIBOOT_BOOTLOADER_MAGIC)
    return 0xD15EA5E;

  /* disable interrupts */
  irq_disable();

  /* init serial console */
  init_serial();

  /* print grub informations */
  printf("[Kernel] Loading at linear address = %x\n", loader);
  kernel_stack = initial_stack;

  /* load initrd file */
  printf("[Kernel] Loading initrd file\n");
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

  /* init PIT */
  printf("[Kernel] PIT Init\n");
  init_pit();

  /* init real time clock */
  printf("[Kernel] Real Time Clock Init\n");
  init_rtc();

  /* init processes */
  printf("[Kernel] Processes Init\n");
  if (init_scheduler(kinit, NULL) != 0)
    panic("Cannot init processes\n");

  /* enable interrupts */
  printf("[Kernel] Enable interrupts\n");
  irq_enable();

  /* code here cannot be reached because the scheduler will always prefer a running task */
  return 0;
}
