#include <x86/gdt.h>
#include <x86/idt.h>
#include <x86/interrupt.h>
#include <x86/io.h>
#include <x86/cpu.h>
#include <mm/mm.h>
#include <grub/multiboot2.h>
#include <drivers/char/mem.h>
#include <drivers/char/serial.h>
#include <drivers/char/pit.h>
#include <drivers/char/rtc.h>
#include <drivers/char/tty.h>
#include <drivers/char/keyboard.h>
#include <drivers/char/mouse.h>
#include <drivers/pci/pci.h>
#include <drivers/block/blk_dev.h>
#include <drivers/block/ata.h>
#include <drivers/block/loop.h>
#include <drivers/video/fb.h>
#include <drivers/net/rtl8139.h>
#include <net/inet/net.h>
#include <ipc/ipc.h>
#include <proc/sched.h>
#include <proc/binfmt.h>
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

#define COMMAND_LINE_SIZE	512
#define ROOT_DEV_NAME_SIZE	32

/* root device */
static dev_t root_dev;
static char root_dev_name[ROOT_DEV_NAME_SIZE] = { 0 };
static int root_mountflags = MS_RDONLY;

extern uint32_t loader;
extern uint32_t kernel_stack;
extern uint32_t kernel_start;
extern uint32_t kernel_end;

/* command line */
char saved_command_line[COMMAND_LINE_SIZE] = { 0 };

/* grub framebuffer */
static struct multiboot_tag_framebuffer tag_fb;

/* static IP address */
static uint8_t default_ip_address[] = { 10, 0, 2, 15 };
static uint8_t default_ip_netmask[] = { 255, 255, 255, 0 };
static uint8_t default_ip_route[] = { 10, 0, 2, 2 };

/*
 * Devices names.
 */
struct dev_name {
	const char *	name;
	dev_t		num;
};

static struct dev_name devices[] = {
	{ "hda",	0x0300 },
	{ "hdb",	0x0310 },
	{ "hdc",	0x0320 },
	{ "hdd",	0x0330 },
	{ NULL,		0x0000 },
};

/*
 * Parse root device.
 */
void parse_root_dev(char *line)
{
	struct dev_name *dev = devices;
	size_t len;

	/* must be a device path */
	if (strncmp(line, "/dev/", 5))
		return;

	/* save root device name */
	strncpy(root_dev_name, line, ROOT_DEV_NAME_SIZE - 1);
	line += 5;

	/* find device */
	for (dev = devices; dev->name; dev++) {
		len = strlen(dev->name);
		if (strncmp(line, dev->name, len) == 0) {
			line += len;
			root_dev = dev->num + (*line - '0');
			return;
		}
	}
}

/*
 * Parse command line.
 */
static void parse_command_line(char *line)
{
	char *next;

	for (next = line; next; line = next) {
		/* end option */
		next = strchr(line, ' ');
		if (next)
			*next++ = 0;

		/* root device option */
		if (strncmp(line, "root=", 5) == 0) {
			parse_root_dev(line + 5);
			continue;
		}

		/* read only option */
		if (strcmp(line, "ro") == 0) {
			root_mountflags |= MS_RDONLY;
			continue;
		}

		/* read write option */
		if (strcmp(line, "rw") == 0) {
			root_mountflags &= ~MS_RDONLY;
			continue;
		}
	}
}

/*
 * Parse multiboot header.
 */
static int parse_mboot(uint32_t mbi_magic, uint32_t mbi_addr, uint32_t *mem_upper)
{
	char command_line[COMMAND_LINE_SIZE] = { 0 };
	struct multiboot_tag *tag;

	/* check magic number */
	if (mbi_magic != MULTIBOOT2_BOOTLOADER_MAGIC)
		return -EINVAL;

	/* check alignement */
	if (mbi_addr & 7)
		return -EINVAL;

	/* parse all multi boot tags */
	for (tag = (struct multiboot_tag *) (mbi_addr + 8);
			 tag->type != MULTIBOOT_TAG_TYPE_END;
			 tag = (struct multiboot_tag *) ((multiboot_uint8_t *) tag + ((tag->size + 7) & ~7))) {

		switch (tag->type) {
			case MULTIBOOT_TAG_TYPE_CMDLINE:
				strncpy(saved_command_line, ((struct multiboot_tag_string *) tag)->string, COMMAND_LINE_SIZE - 1);
				strncpy(command_line, ((struct multiboot_tag_string *) tag)->string, COMMAND_LINE_SIZE - 1);
				printf("Command line = %s\n", saved_command_line);
				parse_command_line(command_line);
				break;
			case MULTIBOOT_TAG_TYPE_BOOT_LOADER_NAME:
				printf("Boot loader name = %s\n", ((struct multiboot_tag_string *) tag)->string);
				break;
			case MULTIBOOT_TAG_TYPE_MODULE:
				printf("Module at 0x%x-0x%x. Command line %s\n",
				       ((struct multiboot_tag_module *) tag)->mod_start,
				       ((struct multiboot_tag_module *) tag)->mod_end,
				       ((struct multiboot_tag_module *) tag)->cmdline);
				break;
			case MULTIBOOT_TAG_TYPE_BASIC_MEMINFO:
				printf("mem_lower = %uKB, mem_upper = %uKB\n",
					((struct multiboot_tag_basic_meminfo *) tag)->mem_lower,
					((struct multiboot_tag_basic_meminfo *) tag)->mem_upper);
				*mem_upper = ((struct multiboot_tag_basic_meminfo *) tag)->mem_upper * 1024;
				break;
			case MULTIBOOT_TAG_TYPE_MMAP:
				init_bios_map((struct multiboot_tag_mmap *) tag);
				break;
			case MULTIBOOT_TAG_TYPE_BOOTDEV:
				printf("Boot device 0x%x,%u,%u\n",
					((struct multiboot_tag_bootdev *) tag)->biosdev,
					((struct multiboot_tag_bootdev *) tag)->slice,
					((struct multiboot_tag_bootdev *) tag)->part);
				break;
			case MULTIBOOT_TAG_TYPE_FRAMEBUFFER:
				memcpy(&tag_fb, (struct multiboot_tag_framebuffer *) tag, tag->size);
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
	/* init mouse */
	printf("[Kernel] Memory devices Init\n");
	if (init_mem_devices())
		printf("[Kernel] Memory devices Init error\n");

	/* init pci devices */
	printf("[Kernel] PCI devices Init\n");
	init_pci();

	/* init keyboard */
	printf("[Kernel] Keyboard Init\n");
	init_keyboard();

	/* init mouse */
	printf("[Kernel] Mouse Init\n");
	if (init_mouse())
		printf("[Kernel] Mouse Init error\n");

	/* init realtek 8139 device */
	printf("[Kernel] Realtek 8139 card Init\n");
	if (init_rtl8139(default_ip_address, default_ip_netmask, default_ip_route) != 0)
		printf("[Kernel] Realtek 8139 card Init error\n");

	/* init block devices */
	printf("[Kernel] Bock devices Init\n");
	init_blk_dev();

	/* init ata devices */
	printf("[Kernel] ATA devices Init\n");
	if (init_ata())
		printf("[Kernel] ATA devices Init error\n");

	/* init loop devices */
	printf("[Kernel] Loop devices Init\n");
	if (init_loop())
		printf("[Kernel] Loop devices Init error\n");

	/* init frame buffer */
	printf("[Kernel] Frame buffer Init\n");
	if (init_framebuffer_device(&tag_fb))
		panic("Cannot init frame buffer");

	/* init ttys */
	printf("[Kernel] Ttys Init\n");
	if (init_tty(&tag_fb))
		panic("Cannot init ttys");

	/* init binary formats */
	printf("[Kernel] Binary formats Init\n");
	init_binfmt();

	/* register filesystems */
	printf("[Kernel] Register file systems\n");
	if (init_minix_fs())
		panic("Cannot register minix file system");
	if (init_ext2_fs())
		panic("Cannot register ext2 file system");
	if (init_proc_fs())
		panic("Cannot register proc file system");
	if (init_tmp_fs())
		panic("Cannot register tmp file system");
	if (init_iso_fs())
		panic("Cannot register iso file system");
	if (init_devpts_fs())
		panic("Cannot register devpts file system");

	/* init network devices */
	printf("[Kernel] Network devices Init\n");
	if (init_net_dev())
		panic("Cannot init network devices");

	/* mount root file system */
	printf("[Kernel] Root file system init\n");
	if (do_mount_root(root_dev, root_dev_name, root_mountflags) != 0)
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
int kmain(uint32_t mbi_magic, uint32_t mbi_addr)
{
	uint32_t mem_upper;
	int ret;

	/* disable interrupts */
	irq_disable();

	/* init serial console */
	init_serial();

	/* parse multiboot header */
	ret = parse_mboot(mbi_magic, mbi_addr, &mem_upper);
	if (ret)
		return ret;

	/* print grub informations */
	printf("[Kernel] Loading at linear address = 0x%x\n", loader);

	/* init cpu */
	printf("[Kernel] CPU Init\n");
	init_cpu();

	/* init gdt */
	printf("[Kernel] Global Descriptor Table Init\n");
	init_gdt();

	/* init idt */
	printf("[Kernel] Interrupt Descriptor Table Init\n");
	init_idt();

	/* init memory */
	printf("[Kernel] Memory Init\n");
	init_mem((uint32_t) &kernel_start, (uint32_t) &kernel_end, mem_upper);

	/* init inodes */
	printf("[Kernel] Inodes init\n");
	init_inode();

	/* init dentries */
	printf("[Kernel] Dentries init\n");
	init_dcache();

	/* init block buffers */
	printf("[Kernel] Block buffers init\n");
	init_buffer();

	/* init IPC resources */
	printf("[Kernel] IPC resources init\n");
	init_ipc();

	/* init PIT */
	printf("[Kernel] PIT Init\n");
	init_pit();

	/* init real time clock */
	printf("[Kernel] Real Time Clock Init\n");
	init_rtc();

	/* init system calls */
	printf("[Kernel] System calls Init\n");
	init_syscall();

	/* init processes */
	printf("[Kernel] Processes Init\n");
	if (init_scheduler(kinit) != 0)
		panic("Cannot init processes");

	return 0;
}
