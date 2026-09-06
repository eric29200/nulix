#include <sys/sys.h>
#include <mm/mm.h>
#include <mm/paging.h>
#include <fs/fs.h>
#include <string.h>
#include <stdio.h>

/*
 * Init memory paging and kernel heap.
 */
void init_mem(uint32_t kernel_start, uint32_t kernel_end, uint32_t mem_end)
{
	int ret;

	/* init paging */
	ret = init_paging(kernel_start, kernel_end, mem_end);
	if (ret)
		panic("Cannot init paging\n");

	/* init heap */
	kheap_init();
}

/*
 * Get informations on memory.
 */
void si_meminfo(struct sysinfo *info)
{
	info->totalram = totalram_pages << PAGE_SHIFT;
	info->freeram = nr_free_pages() << PAGE_SHIFT;
	info->bufferram = buffermem_pages << PAGE_SHIFT;
}

/*
 * Fix mapping to virtual address.
 */
uint32_t fix_to_virt(uint32_t idx)
{
	if (idx >= __end_of_fixed_addresses)
		panic("fix_to_virt: bad idx\n");

        return __fix_to_virt(idx);
}

/*
 * Set a fix mapping.
 */
void set_fixmap(enum fixed_addresses idx, uint32_t phys, uint32_t flags)
{
	uint32_t address = __fix_to_virt(idx);
	pgd_t *pgd;
	pmd_t *pmd;
	pte_t *pte;

	if (idx >= __end_of_fixed_addresses)
		panic("__set_fixmap: bad idx\n");

	pgd = pgd_offset(pgd_kernel, address);
	pmd = pmd_offset(pgd);
	pte = pte_offset(pmd, address);
	*pte = mk_pte_phys(phys, flags);
}