#ifndef _BITOPS_H_
#define _BITOPS_H_

#define test_bit(val, bit)	(((val) & (1UL << (bit))) != 0)
#define set_bit(val, bit)	((val) |= (1UL << (bit)))
#define clear_bit(val, bit)	((val) &= ~(1UL << (bit)))

#endif