#ifndef _APIC_H_
#define _APIC_H_

#include <stddef.h>
#include <mm/mm.h>

#define APIC_BASE 				fix_to_virt(FIX_APIC_BASE)

#define		APIC_TASKPRI			0x80
#define			APIC_TPRI_MASK		0xFF
#define		APIC_EOI			0xB0
#define		APIC_SPIV			0xF0
#define		APIC_LVTT			0x320
#define		APIC_LVT0			0x350
#define 	APIC_LVT1			0x360
#define		APIC_LVTERR			0x370
#define		APIC_TMICT			0x380
#define		APIC_TMCCT			0x390
#define		APIC_TDCR			0x3E0


void init_apic();

/*
 * Write to APIC.
 */
static inline void apic_write(uint32_t reg, uint32_t v)
{
	*((volatile uint32_t *) (APIC_BASE + reg)) = v;
}

/*
 * Read from APIC.
 */
static inline uint32_t apic_read(uint32_t reg)
{
	return *((volatile uint32_t *) (APIC_BASE + reg));
}

#endif