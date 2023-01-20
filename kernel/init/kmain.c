#include <x86/gdt.h>
#include <x86/idt.h>
#include <x86/interrupt.h>
#include <x86/io.h>
#include <mm/mm.h>
#include <grub/multiboot2.h>
#include <drivers/serial.h>
#include <drivers/pit.h>
#include <drivers/rtc.h>
#include <drivers/pci.h>
#include <drivers/ata.h>
#include <drivers/tty.h>
#include <drivers/keyboard.h>
#include <drivers/mouse.h>
#include <drivers/fb.h>
#include <drivers/rtl8139.h>
#include <proc/sched.h>
#include <sys/syscall.h>
#include <fs/minix_fs.h>
#include <fs/ext2_fs.h>
#include <fs/proc_fs.h>
#include <fs/tmp_fs.h>
#include <fs/dev_fs.h>
#include <stdio.h>
#include <string.h>
#include <stderr.h>
#include <fcntl.h>
#include <dev.h>

#define ROOT_DEV	(mkdev(DEV_ATA_MAJOR, 0))
#define ROOT_DEV_NAME	"/dev/hda"

extern uint32_t loader;
extern uint32_t kernel_stack;
extern uint32_t kernel_end;

/* grub framebuffer */
static struct multiboot_tag_framebuffer *tag_fb;

/* static IP address */
static uint8_t default_ip_address[] = { 192, 168, 1, 100 };
static uint8_t default_ip_netmask[] = { 255, 255, 255, 0 };
static uint8_t default_ip_route[] = { 192, 168, 1, 1 };

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
 * Create devices nodes.
 */
static int create_devices_nodes()
{
	char path[64];
	int ret, i;

	/* create disk nodes */
	for (i = 0; i < NR_ATA_DEVICES; i++) {
		sprintf(path, "/dev/hd%c", 'a' + i);
		ret = sys_mknod(path, S_IFBLK | 0600, mkdev(DEV_ATA_MAJOR, i));
		if (ret)
			return ret;
	}

	/* create ttys nodes */
	for (i = 0; i < NR_CONSOLES; i++) {
		sprintf(path, "/dev/tty%d", i);
		ret = sys_mknod(path, S_IFCHR | 0644, DEV_TTY0 + i);
		if (ret)
			return ret;
	}

	/* create /dev/tty node */
	ret = sys_mknod("/dev/tty", S_IFCHR | 0644, DEV_TTY);
	if (ret)
		return ret;

	/* create /dev/console node */
	ret = sys_mknod("/dev/console", S_IFCHR | 0644, DEV_CONSOLE);
	if (ret)
		return ret;
	
	/* create pty slave directory */
	ret = sys_mkdir("/dev/pts", 0755);
	if (ret)
		return ret;

	/* create pty multiplexer */
	ret = sys_mknod("/dev/ptmx", S_IFCHR | 0644, DEV_PTMX);
	if (ret) {
		sys_unlink("/dev/ptmx");
		return ret;
	}

	/* create direct framebuffer node */
	ret = sys_mknod("/dev/fb0", S_IFCHR | 0660, mkdev(DEV_FB_MAJOR, 0));
	if (ret)
		return ret;

	/* create mouse node */
	ret = sys_mknod("/dev/mouse", S_IFCHR | 0660, mkdev(DEV_MOUSE_MAJOR, 0));
	if (ret)
		return ret;

	/* create /dev/input folder */
	ret = sys_mkdir("/dev/input", S_IFDIR | 0755);
	if (ret)
		return ret;

	/* create /dev/input/mice symlink */
	ret = sys_symlink("../mouse", "/dev/input/mice");
	if (ret)
		return ret;

	/* create /dev/zero node */
	ret = sys_mknod("/dev/zero", S_IFCHR | 0666, DEV_ZERO);
	if (ret)
		return ret;

	/* create /dev/random node */
	ret = sys_mknod("/dev/random", S_IFCHR | 0666, DEV_RANDOM);
	if (ret)
		return ret;

	/* create /dev/null node */
	ret = sys_mknod("/dev/null", S_IFCHR | 0666, DEV_NULL);
	if (ret)
		return ret;

	return 0;
}

/*
 * Nulix init (second phase).
 */
static void kinit()
{
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
	if (init_dev_fs() != 0)
		panic("Cannot register device file system");

	/* mount root file system */
	printf("[Kernel] Root file system init\n");
	if (do_mount_root(ROOT_DEV, ROOT_DEV_NAME) != 0)
		panic("Cannot mount root file system");

	/* mount proc file system */
	printf("[Kernel] Proc file system init\n");
	if (sys_mount("proc", "/proc", "proc", MS_RDONLY, NULL) != 0)
		panic("Cannot mount proc file system");

	/* mount tmp file system */
	printf("[Kernel] Tmp file system init\n");
	if (sys_mount("tmp", "/tmp", "tmpfs", MS_RDONLY, NULL) != 0)
		panic("Cannot mount tmp file system");

	/* mount device file system */
	printf("[Kernel] Device file system init\n");
	if (sys_mount("dev", "/dev", "devfs", MS_RDONLY, NULL) != 0)
		panic("Cannot mount device file system");

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
		panic("Cannot init ttys\n");

	/* init direct frame buffer */
	printf("[Kernel] Direct frame buffer Init\n");
	if (init_framebuffer_direct(tag_fb))
		panic("Cannot init direct frame buffer\n");

	/* create devices nodes */
	printf("[Kernel] Create devices nodes\n");
	if (create_devices_nodes())
		panic("Cannot create devices nodes\n");

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

	/* parse multiboot header */
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

	/* init pci devices */
	printf("[Kernel] PCI devices Init\n");
	init_pci();

	/* init ata devices */
	printf("[Kernel] ATA devices Init\n");
	if (init_ata())
		printf("[Kernel] ATA devices Init error\n");

	/* init system calls */
	printf("[Kernel] System calls Init\n");
	init_syscall();

	/* init processes */
	printf("[Kernel] Processes Init\n");
	if (init_scheduler(kinit) != 0)
		panic("Cannot init processes\n");

	/* enable interrupts */
	printf("[Kernel] Enable interrupts\n");
	irq_enable();

	return 0;
}
