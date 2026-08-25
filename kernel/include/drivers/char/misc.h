#ifndef _MISC_DEVICE_H_
#define _MISC_DEVICE_H_

#include <fs/fs.h>
#include <lib/list.h>

#define DEV_MINOR_VIRTIO_RNG		183

/*
 * Misc device.
 */
struct misc_device {
	int				minor;
	const char *			name;
	struct file_operations *	fops;
	struct list_head		list;
};

int init_misc_devices();
int misc_register(struct misc_device * misc);
int misc_deregister(struct misc_device * misc);

#endif