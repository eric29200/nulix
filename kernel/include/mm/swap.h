#ifndef _SWAP_H_
#define _SWAP_H_

#define MAX_SWAPFILES			8
#define SWAP_CLUSTER_MAX		32

#define SWP_USED			1
#define SWP_WRITEOK			3

#define SWP_TYPE(entry)			(((entry) >> 1) & 0x3F)
#define SWP_OFFSET(entry)		((entry) >> 8)
#define SWP_ENTRY(type, offset)		(((type) << 1) | ((offset) << 8))

#define SWAP_FLAG_PREFER		0x8000
#define SWAP_FLAG_PRIO_MASK		0x7FFF
#define SWAP_FLAG_PRIO_SHIFT		0

#define SWAP_MAP_MAX			0x7FFF
#define SWAP_MAP_BAD			0x8000

/*
 * Swap file/device.
 */
struct swap_info {
	uint32_t			type;
	uint32_t			flags;
	dev_t				swap_device;
	struct dentry *			swap_file;
	uint16_t *			swap_map;
	uint32_t			lowest_bit;
	uint32_t			highest_bit;
	int				priority;
	uint32_t			max;
	size_t				pages;
	struct list_head		list;
};

/*
 * Swap header.
 */
union swap_header {
	struct {
		char			reserved[PAGE_SIZE - 10];
		char			magic[10];
	} magic;
	struct {
		char			bootbits[1024];
		uint32_t		version;
		uint32_t		last_page;
		uint32_t		nr_badpages;
		uint32_t		padding[125];
		uint32_t		badpages[1];
	} info;
};

void si_swapinfo(struct sysinfo *info);
void swap_free(uint32_t entry);
void free_page_and_swap_cache(struct page *page);
int swap_out(int priority);
int swap_in(struct vm_area *vma, pte_t *pte, uint32_t entry, int write_access);
int get_swaparea_info(char *buf);
int sys_swapon(const char *path, int swap_flags);
int sys_swapoff(const char *path);

#endif
