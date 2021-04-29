#include <delay.h>
#include <drivers/pit.h>
#include <proc/sched.h>

/*
 * Sleep in milliseonds.
 */
void msleep(uint32_t msecs)
{
  schedule_timeout(ms_to_jiffies(msecs));
}
