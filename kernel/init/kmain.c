#include <x86/gdt.h>
#include <x86/idt.h>
#include <x86/interrupt.h>
#include <x86/io.h>
#include <mm/mm.h>
#include <proc/sched.h>
#include <grub/multiboot2.h>
#include <drivers/serial.h>
#include <drivers/pit.h>
#include <drivers/rtc.h>
#include <drivers/pci.h>
#include <drivers/ata.h>
#include <drivers/tty.h>
#include <drivers/keyboard.h>
#include <drivers/framebuffer.h>
#include <drivers/rtl8139.h>
#include <fs/proc_fs.h>
#include <sys/syscall.h>
#include <stdio.h>
#include <string.h>
#include <stderr.h>
#include <dev.h>

extern uint32_t loader;
extern uint32_t kernel_stack;
extern uint32_t kernel_end;

/* grub framebuffer */
static struct multiboot_tag_framebuffer *tag_fb;

/* static IP address */
static uint8_t default_ip_address[] = {192, 168, 1, 100};
static uint8_t default_ip_netmask[] = {255, 255, 255, 0};
static uint8_t default_ip_route[] = {192, 168, 1, 1};

/*
 * Parse multiboot header.
 */
static int parse_mboot(unsigned long magic, unsigned long addr, uint32_t *mem_upper)
{
  struct multiboot_tag *tag;

  /* check magic number */
  if (magic != MULTIBOOT2_BOOTLOADER_MAGIC)
    return -EINVAL;

  /* check alignement */
  if (addr & 7)
    return -EINVAL;

  /* parse all multi boot tags */
  for (tag = (struct multiboot_tag *) (addr + 8);
       tag->type != MULTIBOOT_TAG_TYPE_END;
       tag = (struct multiboot_tag *) ((multiboot_uint8_t *) tag + ((tag->size + 7) & ~7))) {
    switch (tag->type) {
      case MULTIBOOT_TAG_TYPE_CMDLINE:
        printf("Command line = %s\n", ((struct multiboot_tag_string *) tag)->string);
        break;
      case MULTIBOOT_TAG_TYPE_BOOT_LOADER_NAME:
        printf("Boot loader name = %s\n", ((struct multiboot_tag_string *) tag)->string);
        break;
      case MULTIBOOT_TAG_TYPE_MODULE:
        printf("Module at %x-%x. Command line %s\n",
               ((struct multiboot_tag_module *) tag)->mod_start,
               ((struct multiboot_tag_module *) tag)->mod_end,
               ((struct multiboot_tag_module *) tag)->cmdline);
        break;
      case MULTIBOOT_TAG_TYPE_BASIC_MEMINFO:
        printf ("mem_lower = %uKB, mem_upper = %uKB\n",
                ((struct multiboot_tag_basic_meminfo *) tag)->mem_lower,
                ((struct multiboot_tag_basic_meminfo *) tag)->mem_upper);
        *mem_upper = ((struct multiboot_tag_basic_meminfo *) tag)->mem_upper * 1024;
        break;
      case MULTIBOOT_TAG_TYPE_BOOTDEV:
        printf ("Boot device 0x%x,%u,%u\n",
                ((struct multiboot_tag_bootdev *) tag)->biosdev,
                ((struct multiboot_tag_bootdev *) tag)->slice,
                ((struct multiboot_tag_bootdev *) tag)->part);
        break;
      case MULTIBOOT_TAG_TYPE_FRAMEBUFFER:
        tag_fb = (struct multiboot_tag_framebuffer *) tag;
        break;
    }
  }

  return 0;
}

/*
 * Nulix init (second phase).
 */
static void kinit()
{
  /* mount root file system */
  printf("[Kernel] Root file system init\n");
  if (mount_root(DEV_ATA0) != 0)
    panic("Cannot mount root file system");

  /* mount proc file system */
  printf("[Kernel] Proc file system init\n");
  if (do_mount(PROC_SUPER_MAGIC, 0, "/proc") != 0)
    panic("Cannot mount proc file system");

  /* spawn init process */
  if (spawn_init() != 0)
    panic("Cannot spawn init process");

  /* sleep forever */
  for (;;) {
    current_task->state = TASK_SLEEPING;
    halt();
  }
}

/*
 * Main nulix function.
 */
int kmain(unsigned long magic, unsigned long addr, uint32_t initial_stack)
{
  uint32_t mem_upper;
  int ret;

  /* disable interrupts */
  irq_disable();

  /* init serial console */
  init_serial();

  /* parse multiboot header*/
  ret = parse_mboot(magic, addr, &mem_upper);
  if (ret)
    return ret;

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
  init_mem((uint32_t) &kernel_end, mem_upper);

  /* init PIT */
  printf("[Kernel] PIT Init\n");
  init_pit();

  /* init real time clock */
  printf("[Kernel] Real Time Clock Init\n");
  init_rtc();

  /* init keyboard */
  printf("[Kernel] Keyboard Init\n");
  init_keyboard();

  /* init pci devices */
  printf("[Kernel] PCI devices Init\n");
  init_pci();

  /* init ata devices */
  printf("[Kernel] ATA devices Init\n");
  init_ata();

  /* init realtek 8139 device */
  printf("[Kernel] Realtek 8139 card Init\n");
  if (init_rtl8139(default_ip_address, default_ip_netmask, default_ip_route) != 0)
    printf("[Kernel] Realtek 8139 card Init error\n");

  /* init system calls */
  printf("[Kernel] System calls Init\n");
  init_syscall();

  /* init ttys */
  printf("[Kernel] Ttys Init\n");
  init_tty(tag_fb);

  /* init processes */
  printf("[Kernel] Processes Init\n");
  if (init_scheduler(kinit) != 0)
    panic("Cannot init processes\n");

  /* enable interrupts */
  printf("[Kernel] Enable interrupts\n");
  irq_enable();

  return 0;
}
