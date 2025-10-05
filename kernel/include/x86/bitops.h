#ifndef _BITOPS_H_
#define _BITOPS_H_

/*
 * Test a bit.
 */
static inline int test_bit(uint32_t *addr, uint32_t bit)
{
	int mask;

	addr += bit >> 5;
	mask = 1 << (bit & 0x1F);
	return ((mask & *addr) != 0);
}

/*
 * Set a bit.
 */
static inline int set_bit(uint32_t *addr, uint32_t bit)
{
	int mask, ret;

	addr += bit >> 5;
	mask = 1 << (bit & 0x1F);
	ret = (mask & *addr) != 0;
	*addr |= mask;
	return ret;
}

/*
 * Clear a bit.
 */
static inline int clear_bit(uint32_t *addr, uint32_t bit)
{
	int mask, ret;

	addr += bit >> 5;
	mask = 1 << (bit & 0x1F);
	ret = (mask & *addr) != 0;
	*addr &= ~mask;
	return ret;
}

/*
 * Find first zero bit.
 */
static inline uint32_t find_first_zero_bit(uint32_t *addr, size_t limit)
{
	uint32_t bit;

	for (bit = 0; bit < limit; bit++)
		if (!test_bit(addr, bit))
			return bit;

	return -1;
}

#endif