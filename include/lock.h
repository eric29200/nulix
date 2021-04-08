#ifndef _LOCK_H_
#define _LOCK_H_

#include <stddef.h>
#include <x86/interrupt.h>

/*
 * Spin lock structure.
 */
struct spinlock_t {
  volatile uint32_t lock;
};

#define spin_lock_init(x)                      do { (x)->lock = 0; } while (0)
#define spin_is_locked(x)                      (test_bit(0, (x)))
#define spin_trylock(x)                        (!test_and_set_bit(0, (x)))
#define spin_lock(x)                           do { (x)->lock = 1; } while (0)
#define spin_unlock(x)                         do { (x)->lock = 0; } while (0)

#define spin_lock_irq(x)                       do { irq_disable(); spin_lock(x); } while (0)
#define spin_lock_irqsave(x, flags)            do { irq_save(flags); spin_lock(x); } while (0);
#define spin_unlock_irq(x)                     do { spin_unlock(x); irq_enable(); } while (0);
#define spin_unlock_irqrestore(x, flags)       do { spin_unlock(x); irq_restore(flags); } while (0);

#endif
