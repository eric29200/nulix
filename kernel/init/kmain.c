#include <x86/gdt.h>
#include <x86/idt.h>
#include <x86/interrupt.h>
#include <x86/io.h>
#include <x86/cpu.h>
#include <x86/time.h>
#include <mm/mm.h>
#include <grub/multiboot2.h>
#include <drivers/char/mem.h>
#include <drivers/char/misc.h>
#include <drivers/char/serial.h>
#include <drivers/char/tty.h>
#include <drivers/char/keyboard.h>
#include <drivers/char/mouse.h>
#include <drivers/pci/pci.h>
#include <drivers/block/blk_dev.h>
#include <drivers/block/ide.h>
#include <drivers/block/loop.h>
#include <drivers/video/fb.h>
#include <drivers/net/rtl8139.h>
#include <drivers/net/loopback.h>
#include <drivers/virtio/virtio.h>
#include <net/inet/net.h>
#include <ipc/ipc.h>
#include <proc/sched.h>
#include <proc/binfmt.h>
#include <sys/syscall.h>
#include <fs/minix_fs.h>
#include <fs/cramfs_fs.h>
#include <fs/ext2_fs.h>
#include <fs/proc_fs.h>
#include <fs/tmp_fs.h>
#include <fs/iso_fs.h>
#include <fs/devpts_fs.h>
#include <fs/v9fs_fs.h>
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
				printk("Command line = %s\n", saved_command_line);
				parse_command_line(command_line);
				break;
			case MULTIBOOT_TAG_TYPE_BOOT_LOADER_NAME:
				printk("Boot loader name = %s\n", ((struct multiboot_tag_string *) tag)->string);
				break;
			case MULTIBOOT_TAG_TYPE_MODULE:
				printk("Module at 0x%x-0x%x. Command line %s\n",
					((struct multiboot_tag_module *) tag)->mod_start,
					((struct multiboot_tag_module *) tag)->mod_end,
					((struct multiboot_tag_module *) tag)->cmdline);
				break;
			case MULTIBOOT_TAG_TYPE_BASIC_MEMINFO:
				printk("mem_lower = %uKB, mem_upper = %uKB\n",
					((struct multiboot_tag_basic_meminfo *) tag)->mem_lower,
					((struct multiboot_tag_basic_meminfo *) tag)->mem_upper);
				*mem_upper = ((struct multiboot_tag_basic_meminfo *) tag)->mem_upper * 1024;
				break;
			case MULTIBOOT_TAG_TYPE_MMAP:
				init_bios_map((struct multiboot_tag_mmap *) tag);
				break;
			case MULTIBOOT_TAG_TYPE_BOOTDEV:
				printk("Boot device 0x%x,%u,%u\n",
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
 * Idle task.
 */
static void cpu_idle()
{
	for (;;) {
		/* reschedule if needed */
		if (current->need_resched)
			schedule();

		halt();
	}
}

/*
 * Nulix init (second phase).
 */
static void kinit()
{
	/* init memory devices */
	printk("[Kernel] Memory devices Init\n");
	if (init_mem_devices())
		printk("[Kernel] Memory devices Init error\n");

	/* init misc devices */
	printk("[Kernel] Misc devices Init\n");
	if (init_misc_devices())
		printk("[Kernel] Misc devices Init error\n");

	/* init pci devices */
	printk("[Kernel] PCI devices Init\n");
	if (init_pci())
		printk("[Kernel] PCI devices Init error\n");

	/* init keyboard */
	printk("[Kernel] Keyboard Init\n");
	init_keyboard();

	/* init mouse */
	printk("[Kernel] Mouse Init\n");
	if (init_mouse())
		printk("[Kernel] Mouse Init error\n");

	/* init loopback device */
	printk("[Kernel] Loopback Init\n");
	if (init_loopback())
		printk("[Kernel] Loopback Init error\n");

	/* init realtek 8139 device */
	printk("[Kernel] Realtek 8139 card Init\n");
	if (init_rtl8139())
		printk("[Kernel] Realtek 8139 card Init error\n");

	/* init block devices */
	printk("[Kernel] Bock devices Init\n");
	init_blk_dev();

	/* init ide devices */
	printk("[Kernel] IDE devices Init\n");
	if (init_ide())
		printk("[Kernel] IDE devices Init error\n");

	/* init loop devices */
	printk("[Kernel] Loop devices Init\n");
	if (init_loop())
		printk("[Kernel] Loop devices Init error\n");

	/* init frame buffer */
	printk("[Kernel] Frame buffer Init\n");
	if (init_framebuffer_device(&tag_fb))
		panic("Cannot init frame buffer");

	/* init ttys */
	printk("[Kernel] Ttys Init\n");
	if (init_tty(&tag_fb))
		panic("Cannot init ttys");

	/* init virtio */
	printk("[Kernel] Virtio Init\n");
	if (init_virtio())
		printk("[Kernel] Virtio Init error\n");

	/* init binary formats */
	printk("[Kernel] Binary formats Init\n");
	init_binfmt();

	/* register filesystems */
	printk("[Kernel] Register file systems\n");
	if (init_minix_fs())
		panic("Cannot register minix file system");
	if (init_cramfs_fs())
		panic("Cannot register cramfs file system");
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
	if (init_v9fs_fs())
		panic("Cannot register 9p file system");

	/* init network protocols */
	printk("[Kernel] Init network protocols\n");
	init_proto();

	/* init network devices */
	printk("[Kernel] Network devices Init\n");
	if (init_net_dev())
		panic("Cannot init network devices");

	/* mount root file system */
	printk("[Kernel] Root file system init\n");
	if (do_mount_root(root_dev, root_dev_name, root_mountflags))
		panic("Cannot mount root file system");

	/* spawn init process */
	if (spawn_init())
		panic("Cannot spawn init process");

	/* create kernel threads */
	kernel_thread(&bdflush, NULL, CLONE_FS | CLONE_FILES | CLONE_SIGHAND, "bdflush");
	kernel_thread(&net_handle, NULL, CLONE_FS | CLONE_FILES | CLONE_SIGHAND, "net_handle");

	/* sleep forever */
	cpu_idle();
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
	printk("[Kernel] Loading at linear address = 0x%x\n", loader);

	/* init gdt */
	printk("[Kernel] Global Descriptor Table Init\n");
	init_gdt();

	/* init idt */
	printk("[Kernel] Interrupts Init\n");
	init_irq();

	/* init memory */
	printk("[Kernel] Memory Init\n");
	init_mem((uint32_t) &kernel_start, (uint32_t) &kernel_end, mem_upper);

	/* init cpu */
	printk("[Kernel] CPU Init\n");
	init_cpu();

	/* init time */
	printk("[Kernel] Time Init\n");
	init_time();

	/* init inodes */
	printk("[Kernel] Inodes init\n");
	init_inode();

	/* init dentries */
	printk("[Kernel] Dentries init\n");
	init_dcache();

	/* init block buffers */
	printk("[Kernel] Block buffers init\n");
	init_buffer();

	/* init IPC resources */
	printk("[Kernel] IPC resources init\n");
	init_ipc();

	/* init system calls */
	printk("[Kernel] System calls Init\n");
	init_syscall();

	/* init processes */
	printk("[Kernel] Processes Init\n");
	if (init_scheduler(kinit))
		panic("Cannot init processes\n");

	return 0;
}
