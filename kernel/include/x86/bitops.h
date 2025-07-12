#ifndef _BITOPS_H_
#define _BITOPS_H_

static inline int test_bit(uint32_t *addr, int bit)
{
	return (*addr & (1UL << bit)) != 0;
}

static inline int set_bit(uint32_t *addr, int bit)
{
	int old = test_bit(addr, bit);
	*addr |= 1UL << bit;
	return old;
}

static inline int clear_bit(uint32_t *addr, int bit)
{
	int old = test_bit(addr, bit);
	*addr &= ~(1UL << bit);
	return old;
}

#endif