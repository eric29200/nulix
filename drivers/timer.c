#include <kernel/interrupt.h>
#include <kernel/io.h>
#include <kernel/mm.h>
#include <drivers/timer.h>
#include <stdio.h>
#include <stderr.h>

/* current jiffies */
volatile uint32_t jiffies = 0;

/* timer events */
struct timer_event_t *timer_events = NULL;

/*
 * Timer handler.
 */
static void timer_handler(struct registers_t regs)
{
  struct timer_event_t *e;

  UNUSED(regs);

  /* update jiffies */
  jiffies++;

  /* execute timers */
  for (e = timer_events; e != NULL; e = e->next) {
    if (e->jiffies <= jiffies) {
      /* execute timer event */
      e->handler(e->handler_args);

      /* remove timer event */
      if (e->next)
        e->next->prev = e->prev;
      if (e->prev)
        e->prev->next = e->next;
      else
        timer_events = e->next;

      /* free memory */
      kfree(e);
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
}

/*
 * Add a timer event.
 */
int timer_add_event(uint32_t sec, void (*handler)(void *), void *handler_args)
{
  struct timer_event_t *event, *e;
  uint32_t flags;

  /* create a new timer event */
  event = (struct timer_event_t *) kmalloc(sizeof(struct timer_event_t));
  if (!event)
    return ENOMEM;

  /* set timer event */
  event->jiffies = jiffies + sec * HZ;
  event->handler = handler;
  event->handler_args = handler_args;
  event->prev = NULL;
  event->next = NULL;

  /* add to the list */
  irq_save(flags);
  for (e = timer_events; e != NULL && e->next != NULL; e = e->next);
  if (e == NULL) {
    timer_events = event;
  } else {
    e->next = event;
    event->prev =  e;
  }
  irq_restore(flags);

  return 0;
}
