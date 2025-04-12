#include <x86/gdt.h>
#include <x86/idt.h>
#include <x86/interrupt.h>
#include <x86/io.h>
#include <mm/mm.h>
#include <grub/multiboot2.h>
#include <drivers/char/serial.h>
#include <drivers/char/pit.h>
#include <drivers/char/rtc.h>
#include <drivers/char/tty.h>
#include <drivers/char/keyboard.h>
#include <drivers/char/mouse.h>
#include <drivers/pci/pci.h>
#include <drivers/block/ata.h>
#include <drivers/video/fb.h>
#include <drivers/net/rtl8139.h>
#include <proc/sched.h>
#include <sys/syscall.h>
#include <fs/minix_fs.h>
#include <fs/ext2_fs.h>
#include <fs/proc_fs.h>
#include <fs/tmp_fs.h>
#include <fs/iso_fs.h>
#include <fs/devpts_fs.h>
#include <stdio.h>
#include <string.h>
#include <stderr.h>
#include <fcntl.h>
#include <dev.h>

#define ROOT_DEV		(mkdev(DEV_ATA_MAJOR, 1))
#define ROOT_DEV_NAME		"/dev/hda1"

extern uint32_t loader;
extern uint32_t kernel_stack;
extern uint32_t kernel_end;

/* grub framebuffer */
static struct multiboot_tag_framebuffer *tag_fb;

/* static IP address */
static uint8_t default_ip_address[] = { 10, 0, 2, 15 };
static uint8_t default_ip_netmask[] = { 255, 255, 255, 0 };
static uint8_t default_ip_route[] = { 10, 0, 2, 2 };

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
	if (do_mount_root(ROOT_DEV, ROOT_DEV_NAME) != 0)
		panic("Cannot mount root file system");

	/* spawn init process */
	if (spawn_init() != 0)
		panic("Cannot spawn init process");

	/* sleep forever */
	for (;;)
		halt();
}

/*
 * Main nulix function.
 */
int kmain(unsigned long magic, unsigned long addr)
{
	uint32_t mem_upper;
	int ret;

	/* disable interrupts */
	irq_disable();

	/* init serial console */
	init_serial();

	/* parse multiboot header */
	ret = parse_mboot(magic, addr, &mem_upper);
	if (ret)
		return ret;

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
	init_mem((uint32_t) &kernel_end, mem_upper);

	/* init inodes */
	printf("[Kernel] Inodes init\n");
	if (iinit() != 0)
		panic("Cannot allocate memory for inodes");

	/* init block buffers */
	printf("[Kernel] Block buffers init\n");
	if (binit() != 0)
		panic("Cannot allocate memory for block buffers");

	/* init PIT */
	printf("[Kernel] PIT Init\n");
	init_pit();

	/* init real time clock */
	printf("[Kernel] Real Time Clock Init\n");
	init_rtc();

	/* init system calls */
	printf("[Kernel] System calls Init\n");
	init_syscall();

	/* init pci devices */
	printf("[Kernel] PCI devices Init\n");
	init_pci();

	/* init keyboard */
	printf("[Kernel] Keyboard Init\n");
	init_keyboard();

	/* init mouse */
	printf("[Kernel] Mouse Init\n");
	if (init_mouse() != 0)
		printf("[Kernel] Cannot init mouse\n");

	/* init realtek 8139 device */
	printf("[Kernel] Realtek 8139 card Init\n");
	if (init_rtl8139(default_ip_address, default_ip_netmask, default_ip_route) != 0)
		printf("[Kernel] Realtek 8139 card Init error\n");

	/* init ttys */
	printf("[Kernel] Ttys Init\n");
	if (init_tty(tag_fb))
		panic("Cannot init ttys");

	/* init direct frame buffer */
	printf("[Kernel] Direct frame buffer Init\n");
	if (init_framebuffer_direct(tag_fb))
		panic("Cannot init direct frame buffer");

	/* init ata devices */
	printf("[Kernel] ATA devices Init\n");
	if (init_ata())
		printf("[Kernel] ATA devices Init error\n");

	/* register filesystems */
	printf("[Kernel] Register file systems\n");
	if (init_minix_fs() != 0)
		panic("Cannot register minix file system");
	if (init_ext2_fs() != 0)
		panic("Cannot register ext2 file system");
	if (init_proc_fs() != 0)
		panic("Cannot register proc file system");
	if (init_tmp_fs() != 0)
		panic("Cannot register tmp file system");
	if (init_iso_fs() != 0)
		panic("Cannot register iso file system");
	if (init_devpts_fs() != 0)
		panic("Cannot register devpts file system");

	/* init processes */
	printf("[Kernel] Processes Init\n");
	if (init_scheduler(kinit) != 0)
		panic("Cannot init processes");

	return 0;
}
