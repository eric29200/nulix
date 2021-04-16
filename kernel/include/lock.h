#ifndef _LOCK_H_
#define _LOCK_H_

#include <stddef.h>
#include <x86/interrupt.h>

#define SPIN_LOCK_UNLOCKED 0
#define SPIN_LOCK_LOCK     1

typedef unsigned char spinlock_t;

#define barrier()                              __asm__ __volatile__("" : : : "memory")
#define cpu_relax()                            __asm__ __volatile__("pause" : : : "memory")

/*
 * Atomic operation to test and set.
 */
static inline unsigned short xchg_8(void *ptr, unsigned char x)
{
	__asm__ __volatile__("xchgb %0,%1"
						 : "=r" (x)
						 : "m"(*(volatile unsigned char *) ptr), "0" (x)
						 : "memory");

	return x;
}

/*
 * Lock.
 */
static inline void spin_lock(spinlock_t *lock)
{
	while (1) {
		if (!xchg_8(lock, SPIN_LOCK_LOCK))
			return;

		while (*lock)
			cpu_relax();
	}
}

/*
 * Unlock.
 */
static inline void spin_unlock(spinlock_t *lock)
{
	barrier();
	*lock = 0;
}

/*
 * Try to lock.
 */
static inline int spin_trylock(spinlock_t *lock)
{
	return xchg_8(lock, SPIN_LOCK_LOCK);
}


#define spin_lock_init(x)	                     do { *(x) = SPIN_LOCK_UNLOCKED; } while(0)

#define spin_lock_irq(x)                       do { irq_disable(); spin_lock(x); } while (0)
#define spin_lock_irqsave(x, flags)            do { irq_save(flags); spin_lock(x); } while (0);
#define spin_unlock_irq(x)                     do { spin_unlock(x); irq_enable(); } while (0);
#define spin_unlock_irqrestore(x, flags)       do { spin_unlock(x); irq_restore(flags); } while (0);

#endif
