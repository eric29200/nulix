#ifndef _SMP_H_
#define _SMP_H_

#include <stddef.h>

#define SMP_MAGIC_IDENT		(('_' << 24) | ('P' << 16) | ('M' << 8) | '_')
#define MPC_SIGNATURE		"PCMP"

#define	MP_PROCESSOR		0
#define	MP_BUS			1
#define	MP_IOAPIC		2
#define	MP_INTSRC		3
#define	MP_LINTSRC		4

/*
 * Intel MP table.
 */
struct intel_mp_floating {
	char		mpf_signature[4];		/* "_MP_" */
	uint32_t	mpf_physptr;			/* Configuration table address */
	uint8_t		mpf_length;			/* Our length (paragraphs) */
	uint8_t		mpf_specification;		/* Specification version */
	uint8_t		mpf_checksum;			/* Checksum (makes sum 0) */
	uint8_t		mpf_feature1;			/* Standard or configuration ? */
	uint8_t		mpf_feature2;			/* Bit7 set for IMCR|PIC */
	uint8_t		mpf_feature3;			/* Unused */
	uint8_t		mpf_feature4;			/* Unused */
	uint8_t		mpf_feature5;			/* Unused */
} __attribute__((packed));

/*
 * Intel MP config table.
 */
struct mp_config_table {
	char		mpc_signature[4];
	uint16_t	mpc_length;			/* Size of table */
	char		mpc_spec;			/* 0x01 */
	char		mpc_checksum;
	char		mpc_oem[8];
	char		mpc_productid[12];
	uint32_t	mpc_oemptr;			/* 0 if not present */
	uint16_t	mpc_oemsize;			/* 0 if not present */
	uint16_t	mpc_oemcount;
	uint32_t	mpc_lapic;			/* APIC address */
	uint32_t	reserved;
} __attribute__((packed));

void init_smp();

#endif