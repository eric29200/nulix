#ifndef _INTERRUPT_H_
#define _INTERRUPT_H_

#include <stddef.h>

#define IRQ0    32
#define IRQ1    33
#define IRQ2    34
#define IRQ3    35
#define IRQ4    36
#define IRQ5    37
#define IRQ6    38
#define IRQ7    39
#define IRQ8    40
#define IRQ9    41
#define IRQ10   42
#define IRQ11   43
#define IRQ12   44
#define IRQ13   45
#define IRQ14   46
#define IRQ15   47

struct registers_t {
  uint32_t ds;
  uint32_t edi;
  uint32_t esi;
  uint32_t ebp;
  uint32_t esp;
  uint32_t ebx;
  uint32_t edx;
  uint32_t ecx;
  uint32_t eax;
  uint32_t int_no;
  uint32_t err_code;
  uint32_t eip;
  uint32_t cs;
  uint32_t eflags;
  uint32_t useresp;
  uint32_t ss;
};

#define __save_flags(x)       __asm__ __volatile__("pushfl ; popl %0":"=g" (x):)
#define __restore_flags(x) 	  __asm__ __volatile__("pushl %0 ; popfl": :"g" (x):"memory", "cc")
#define irq_enable()          __asm__ __volatile__("sti": : :"memory")
#define irq_disable()         __asm__ __volatile__("cli": : :"memory")
#define irq_save(x)           do { __save_flags(x); irq_disable(); } while(0);
#define irq_restore(x)        __restore_flags(x);

/* common ISR and IRQ handlers */
void isr_handler(struct registers_t regs);
void irq_handler(struct registers_t regs);

/* IRQ handler registration */
typedef void (*isr_t)(struct registers_t);
void register_interrupt_handler(uint8_t n, isr_t handler);


#endif
