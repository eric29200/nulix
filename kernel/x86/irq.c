#include <x86/interrupt.h>
#include <kernel_stat.h>
#include <x86/io.h>
#include <sys/syscall.h>
#include <proc/sched.h>
#include <kernel_stat.h>
#include <stderr.h>
#include <stdio.h>

/* IRQ descriptors */
static irq_desc_t irq_desc[NR_IRQS] = { NULL, };

/*
 * Request an IRQ.
 */
int request_irq(uint32_t irq, void *handler, const char *devname)
{
	struct irq_action *action;

	/* check irq */
	if (irq >= NR_IRQS)
		return -EINVAL;

	/* check handler */
	if (!handler)
		return -EINVAL;

	/* handler already installed */
	if (irq_desc[irq].action)
		return -EBUSY;

	/* allocate a new irq action */
	action = (struct irq_action *) kmalloc(sizeof(struct irq_action));
	if (!action)
		return -ENOMEM;

	/* set action */
	action->handler = handler;
	action->name = devname;

	/* install IRQ */
	irq_desc[irq].action = action;

	return 0;
}

/*
 * Free an IRQ.
 */
void free_irq(uint32_t irq)
{
	/* check irq */
	if (irq >= NR_IRQS || !irq_desc[irq].action)
		return;

	/* free irq */
	kfree(irq_desc[irq].action);
	irq_desc[irq].action = NULL;
}

/*
 * IRQ handler.
 */
void irq_handler(struct registers *regs)
{
	int irq = regs->int_no;

	/* update kernel statistics */
	kstat.irqs[irq]++;

	/* send reset signal to slave PIC (if irq > 7) */
	if (irq > 7)
		outb(0xA0, 0x20);

	/* send reset signal to master PIC */
	outb(0x20, 0x20);

	/* handle interrupt */
	if (irq_desc[irq].action)
		irq_desc[irq].action->handler(regs);
}

/*
 * Get IRQ list.
 */
size_t get_irq_list(char *page)
{
	struct irq_action *action;
	char *ptr = page;
	size_t i;

	/* print header */
	ptr += sprintf(ptr, "           CPU0       \n");

	/* for each irq */
	for (i = 0 ; i < NR_IRQS ; i++) {
		/* get action */
		action = irq_desc[i].action;
		if (!action) 
			continue;

		/* print irq */
		ptr += sprintf(ptr, "%3d: %10u   %s\n", i, kstat.irqs[i], action->name);
	}

	return ptr - page;
}