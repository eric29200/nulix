#include <kernel/interrupt.h>
#include <kernel/io.h>
#include <kernel/mm.h>
#include <kernel/system.h>
#include <drivers/timer.h>
#include <stdio.h>
#include <stderr.h>

/* current jiffies */
volatile uint32_t jiffies = 0;

/* timer events */
struct list_t timer_events;

/*
 * Timer handler.
 */
static void timer_handler(struct registers_t regs)
{
  struct timer_event_t *event;
  struct list_t *elt;

  UNUSED(regs);

  /* update jiffies */
  jiffies++;

  /* for each timers */
  list_for_each(elt, &timer_events) {
    event = list_entry(elt, struct timer_event_t, list);

    /* execute timer */
    if (event->jiffies <= jiffies) {
      event->handler(event->handler_args);

      /* remove it from list and free memory */
      list_del(&event->list);
      kfree(event);
    }
  }
}

/*
 * Init the Programmable Interval Timer.
 */
void init_timer()
{
  uint32_t divisor;
  uint8_t divisor_low;
  uint8_t divisor_high;

  /* register irq */
  register_interrupt_handler(IRQ0, &timer_handler);

  /* set frequency (PIT's clock is always at 1193180 Hz) */
  divisor = 1193182 / HZ;
  divisor_low = (uint8_t) (divisor & 0xFF);
  divisor_high = (uint8_t) ((divisor >> 8) & 0xFF);

  /* send command and frequency divisor */
  outb(0x43, 0x36);
  outb(0x40, divisor_low);
  outb(0x40, divisor_high);

  /* init timer events list */
  list_init(&timer_events);
}

/*
 * Add a timer event.
 */
int timer_add_event(uint32_t sec, void (*handler)(void *), void *handler_args)
{
  struct timer_event_t *event;
  uint32_t flags;

  /* create a new timer event */
  event = (struct timer_event_t *) kmalloc(sizeof(struct timer_event_t));
  if (!event)
    return ENOMEM;

  /* set timer event */
  event->jiffies = jiffies + sec * HZ;
  event->handler = handler;
  event->handler_args = handler_args;

  /* add to the list */
  irq_save(flags);
  list_add(&event->list, &timer_events);
  irq_restore(flags);

  return 0;
}
