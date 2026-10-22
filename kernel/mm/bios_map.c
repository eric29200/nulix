#include <grub/multiboot2.h>
#include <string.h>
#include <stdio.h>
#include <stderr.h>

#define NR_BIOS_MAP_ENTRIES	32

/*
 * Bios map structure.
 */
struct bios_map {
	uint32_t	start;
	uint32_t	end;
	int 		type;
};

/* bios map */
static struct bios_map bios_map[NR_BIOS_MAP_ENTRIES];
static size_t nr_entries = 0;

/*
 * Is an address available ?
 */
int bios_map_address_available(uint32_t addr)
{
	int ret = 0;
	size_t n;

	for (n = 0; n < nr_entries; n++)
		if (addr >= bios_map[n].start && addr < bios_map[n].end && bios_map[n].type == MULTIBOOT_MEMORY_AVAILABLE)
			ret = 1;

	for (n = 0; n < nr_entries; n++)
		if (addr >= bios_map[n].start && addr < bios_map[n].end && bios_map[n].type != MULTIBOOT_MEMORY_AVAILABLE)
			ret = 0;

	return ret;
}

/*
 * Add an entry.
 */
int bios_map_add_entry(uint32_t start, uint32_t end, int type)
{
	/* no available entry */
	if (nr_entries >= NR_BIOS_MAP_ENTRIES)
		return -ENOENT;

	/* add entry */
	bios_map[nr_entries].start = start;
	bios_map[nr_entries].end = end;
	bios_map[nr_entries].type = type;
	nr_entries++;

	return 0;
}

/*
 * Init bios map.
 */
void init_bios_map(struct multiboot_tag_mmap *mbi_mmap, uint32_t *mem_upper)
{
	uint32_t start_low, start_high, end_low, end_high;
	struct multiboot_mmap_entry *entry;
	uint8_t *ptr, *end;

	/* reset bios map */
	*mem_upper = 0;
	memset(bios_map, 0, sizeof(struct bios_map) * NR_BIOS_MAP_ENTRIES);

	/* for each entry */
	ptr = (uint8_t *) mbi_mmap->entries;
	end = (uint8_t *) mbi_mmap + mbi_mmap->size;
	while ((size_t) (end - ptr) >= mbi_mmap->entry_size) {
		entry = (struct multiboot_mmap_entry *) ptr;

		/* compute start address */
		start_low = entry->addr & 0xFFFFFFFF;
		start_high = entry->addr >> 32;

		/* compute end address */
		end_low = (entry->addr + entry->len) & 0xFFFFFFFF;
		end_high = (entry->addr + entry->len) >> 32;

		/* add mapping */
		if (!start_high && !end_high && nr_entries < NR_BIOS_MAP_ENTRIES) {
			bios_map[nr_entries].start = start_low;
			bios_map[nr_entries].end = end_low;
			bios_map[nr_entries].type = entry->type;
			nr_entries++;
		}

		/* update total memory size */
		if (entry->type == MULTIBOOT_MEMORY_AVAILABLE) {
			if (UINT_MAX - *mem_upper < entry->len)
				*mem_upper = UINT_MAX;
			else
				*mem_upper += entry->len;
		}

		/* go to next entry */
		ptr += mbi_mmap->entry_size;
	}
}
