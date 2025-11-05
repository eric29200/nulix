#ifndef _INTERRUPT_H_
#define _INTERRUPT_H_

#include <x86/system.h>
#include <lib/list.h>
#include <stddef.h>

#define NR_EXCEPTIONS		129
#define NR_IRQS			224

#define __save_flags(x)		__asm__ __volatile__("pushfl ; popl %0":"=g" (x):)
#define __restore_flags(x)	__asm__ __volatile__("pushl %0 ; popfl": :"g" (x):"memory", "cc")
#define irq_enable()		__asm__ __volatile__("sti": : :"memory")
#define irq_disable()		__asm__ __volatile__("cli": : :"memory")
#define irq_save(x)		do { __save_flags(x); irq_disable(); } while (0);
#define irq_restore(x)		__restore_flags(x);

/*
 * IRQ action.
 */
struct irq_action {
	void 			(*handler)(struct registers *);
};

/*
 * IRQ descriptor.
 */
typedef struct {
	struct irq_action *	action;
} irq_desc_t;

/* exception and irq handlers */
void exception_handler(struct registers *regs);
void irq_handler(struct registers *regs);

/* IRQ handler registration */
void register_exception_handler(uint32_t n, void *handler);
int request_irq(uint32_t irq, void *handler);
void free_irq(uint32_t irq);

#endif
