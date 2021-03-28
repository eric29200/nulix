#ifndef _ISR_H_
#define _ISR_H_

#include <lib/stddef.h>

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

/* common ISR and IRQ handlers */
void isr_handler(struct registers_t regs);
void irq_handler(struct registers_t regs);

/* IRQ handler registration */
typedef void (*isr_t)(struct registers_t);
void register_interrupt_handler(uint8_t n, isr_t handler);

#endif
