#ifndef _IO_APIC_H_
#define _IO_APIC_H_

#include <mm/mm.h>
#include <mm/paging.h>

#define IO_APIC_BASE(idx) 	((volatile int *) (__fix_to_virt(FIX_IO_APIC_BASE_0 + idx) + (mp_ioapics[idx].mpc_apicaddr & ~PAGE_MASK)))

/* extern variables */
extern struct mpc_config_ioapic mp_ioapics[MAX_IO_APICS];
extern int nr_ioapics;

/* prototypes */
void init_io_apic();

/*
 * Read from I/O APIC.
 */
static inline uint32_t io_apic_read(uint32_t apic, uint32_t reg)
{
	*IO_APIC_BASE(apic) = reg;
	return *(IO_APIC_BASE(apic) + 4);
}

/*
 * Write to I/O APIC.
 */
static inline void io_apic_write(uint32_t apic, uint32_t reg, uint32_t value)
{
	*IO_APIC_BASE(apic) = reg;
	*(IO_APIC_BASE(apic) + 4) = value;
}

#endif