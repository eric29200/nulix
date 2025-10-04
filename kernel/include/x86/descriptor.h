#ifndef _DESCRIPTOR_H_
#define _DESCRIPTOR_H_

#include <stddef.h>
#include <string.h>

/*
 * Global Descriptor Table entry.
 */
struct desc_struct {
	uint16_t	limit0;
	uint16_t	base0;
	uint16_t	base1:8;
	uint16_t	type:4;
	uint16_t	s:1;
	uint16_t	dpl:2;
	uint16_t	p:1;
	uint16_t	limit1:4;
	uint16_t	avl:1;
	uint16_t	l:1;
	uint16_t	d:1;
	uint16_t	g:1;
	uint16_t	base2:8;
} __attribute__((packed));

/*
 * User Descriptor.
 */
struct user_desc {
	uint32_t	entry_number;
	uint32_t	base_addr;
	uint32_t	limit;
	uint32_t	seg_32bit:1;
	uint32_t	contents:2;
	uint32_t	read_exec_only:1;
	uint32_t	limit_in_pages:1;
	uint32_t	seg_not_present:1;
	uint32_t	useable:1;
};

/*
 * LDT/TSS Descriptor.
 */
struct ldttss_desc {
	uint16_t	limit0;
	uint16_t	base0;
	uint16_t	base1:8;
	uint16_t	type:5;
	uint16_t	dpl:2;
	uint16_t	p:1;
	uint16_t	limit1:4;
	uint16_t	zero0:3;
	uint16_t	g:1;
	uint16_t	base2:8;
} __attribute__((packed));


/*
 * Descriptors.
 */
enum {
	DESC_TSS = 0x9,
	DESC_LDT = 0x2,
	DESCTYPE_S = 0x10,
};

/*
 * Is a Local Descriptor Empty ?
 */
#define LDT_empty(info)					\
	((info)->base_addr		== 0	&&	\
	 (info)->limit			== 0	&&	\
	 (info)->contents		== 0	&&	\
	 (info)->read_exec_only		== 1	&&	\
	 (info)->seg_32bit		== 0	&&	\
	 (info)->limit_in_pages		== 0	&&	\
	 (info)->seg_not_present	== 1	&&	\
	 (info)->useable		== 0)

/*
 * Is a Local Descriptor Zero ?
 */
static inline int LDT_zero(struct user_desc *info)
{
	return (info->base_addr		== 0 &&
		info->limit		== 0 &&
		info->contents		== 0 &&
		info->read_exec_only	== 0 &&
		info->seg_32bit		== 0 &&
		info->limit_in_pages	== 0 &&
		info->seg_not_present	== 0 &&
		info->useable		== 0);
}

/*
 * Fill in a Local Descriptor Table.
 */
static inline void fill_ldt(struct desc_struct *desc, struct user_desc *info)
{
	desc->limit0		= info->limit & 0x0FFFF;
	desc->base0		= (info->base_addr & 0x0000FFFF);
	desc->base1		= (info->base_addr & 0x00FF0000) >> 16;
	desc->type		= (info->read_exec_only ^ 1) << 1;
	desc->type	       |= info->contents << 2;
	desc->type	       |= 1;
	desc->s			= 1;
	desc->dpl		= 0x3;
	desc->p			= info->seg_not_present ^ 1;
	desc->limit1		= (info->limit & 0xf0000) >> 16;
	desc->avl		= info->useable;
	desc->d			= info->seg_32bit;
	desc->g			= info->limit_in_pages;
	desc->base2		= (info->base_addr & 0xff000000) >> 24;
	desc->l			= 0;
}

/*
 * Set a TSS/LDT descriptor.
 */
static inline void set_tssldt_descriptor(void *d, uint32_t addr, uint32_t type, uint32_t size)
{
	struct ldttss_desc *desc = d;

	memset(desc, 0, sizeof(*desc));

	desc->limit0		= (uint16_t) size;
	desc->base0		= (uint16_t) addr;
	desc->base1		= (addr >> 16) & 0xFF;
	desc->type		= type;
	desc->p			= 1;
	desc->limit1		= (size >> 16) & 0xF;
	desc->base2		= (addr >> 24) & 0xFF;
}

/*
 * Get descriptor base address.
 */
static inline uint32_t get_desc_base(struct desc_struct *desc)
{
	return (uint32_t) (desc->base0 | ((desc->base1) << 16) | ((desc->base2) << 24));
}

/*
 * Get descriptor limit.
 */
static inline uint32_t get_desc_limit(struct desc_struct *desc)
{
	return desc->limit0 | (desc->limit1 << 16);
}

#endif
