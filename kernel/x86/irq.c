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
int request_irq(uint32_t irq, void *handler, uint32_t flags, const char *devname, void *dev_id)
{
	struct irq_action *action, *old;

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
	action->flags = flags;
	action->name = devname;
	action->dev_id = dev_id;
	action->next = NULL;

	/* install IRQ */
	old = irq_desc[irq].action;
	if (!old) {
		irq_desc[irq].action = action;
		return 0;
	}

	/* can't share irq */
	if (!(old->flags & action->flags & SA_SHIRQ)) {
		kfree(action);
		return -EBUSY;
	}

	/* install shared IRQ */
	for (; old->next; old = old->next);
	old->next = action;

	return 0;
}

/*
 * Free an IRQ.
 */
void free_irq(uint32_t irq, void *dev_id)
{
	struct irq_action *action, **p;

	/* check irq */
	if (irq >= NR_IRQS)
		return;

	/* find irq to free */
	for (p = &irq_desc[irq].action; (action = *p) != NULL; p = &action->next) {
		if (action->dev_id != dev_id)
			continue;

		*p = action->next;
		kfree(action);
	}
}

/*
 * IRQ handler.
 */
void irq_handler(struct registers *regs)
{
	struct irq_action *action;
	int irq = regs->int_no;

	/* update kernel statistics */
	kstat.irqs[irq]++;

	/* send reset signal to slave PIC (if irq > 7) */
	if (irq > 7)
		outb(0xA0, 0x20);

	/* send reset signal to master PIC */
	outb(0x20, 0x20);

	/* handle interrupt */
	for (action = irq_desc[irq].action; action != NULL; action = action->next)
		action->handler(regs);
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
		ptr += sprintf(ptr, "%3d: %10u   %s", i, kstat.irqs[i], action->name);
		for (action = action->next; action != NULL; action = action->next)
			ptr += sprintf(ptr, ", %s", action->name);
		*ptr++ = '\n';
	}

	return ptr - page;
}
