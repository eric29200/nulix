#ifndef _INTERRUPT_H_
#define _INTERRUPT_H_

#include <x86/system.h>
#include <stddef.h>

#define __save_flags(x)		__asm__ __volatile__("pushfl ; popl %0":"=g" (x):)
#define __restore_flags(x)	__asm__ __volatile__("pushl %0 ; popfl": :"g" (x):"memory", "cc")
#define irq_enable()		__asm__ __volatile__("sti": : :"memory")
#define irq_disable()		__asm__ __volatile__("cli": : :"memory")
#define irq_save(x)		do { __save_flags(x); irq_disable(); } while (0);
#define irq_restore(x)		__restore_flags(x);

/* exception and irq handlers */
void exception_handler(struct registers *regs);
void irq_handler(struct registers *regs);

/* IRQ handler registration */
typedef void (*isr_t)(struct registers *);
void register_exception_handler(uint32_t n, isr_t handler);
void register_interrupt_handler(uint32_t n, isr_t handler);
void unregister_interrupt_handler(uint32_t n);

#endif
