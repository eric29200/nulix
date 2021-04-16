#ifndef _BITOPS_H_
#define _BITOPS_H_

#define ADDR      (*(volatile long *) addr)

/*
 * Test bit macro (from linux) = atomic.
 */
static __inline__ int test_bit(int nr, const volatile void * addr)
{
	return ((1UL << (nr & 31)) & (((const volatile unsigned int *) addr)[nr >> 5])) != 0;
}

/*
 * Test and set bit macro (from linux) = atomic.
 */
static __inline__ int test_and_set_bit(int nr, volatile void * addr)
{
	int oldbit;

	__asm__ __volatile__(
		"btsl %2,%1\n\tsbbl %0,%0"
		:"=r" (oldbit),"=m" (ADDR)
		:"Ir" (nr) : "memory");

	return oldbit;
}

#endif
