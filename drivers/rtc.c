#include <drivers/rtc.h>
#include <kernel/io.h>
#include <time.h>

uint32_t startup_time = 0;

/*
 * Get real time clock.
 */
static uint32_t rtc_get_time()
{
  uint32_t year, month, day, hour, min, sec;
  int i;

  /* wait for the UIP flag (Update In Progress) to go from 1 to 0 = the RTC registers should hold the precise second */
  for (i = 0; i < 1000000; i++)
    if (CMOS_READ(RTC_FREQ_SELECT) & RTC_UIP)
      break;

  for (i = 0; i < 1000000; i++)
    if (!(CMOS_READ(RTC_FREQ_SELECT) & RTC_UIP))
      break;

  /* read time from cmos */
  do {
    sec = CMOS_READ(RTC_SECOND);
    min = CMOS_READ(RTC_MINUTE);
    hour = CMOS_READ(RTC_HOUR);
    day = CMOS_READ(RTC_DAY);
    month = CMOS_READ(RTC_MONTH);
    year = CMOS_READ(RTC_YEAR);
  } while (sec != CMOS_READ(RTC_SECOND));

  /* convert BCD time to BIN time if needed */
  if (!(CMOS_READ(RTC_CONTROL) & RTC_DM_BINARY) || RTC_ALWAYS_BCD) {
    BCD_TO_BIN(sec);
    BCD_TO_BIN(min);
    BCD_TO_BIN(hour);
    BCD_TO_BIN(day);
    BCD_TO_BIN(month);
    BCD_TO_BIN(year);
  }

  /* update year */
  if ((year += 1900) < 1970)
    year += 100;

  return mktime(year, month, day, hour, min, sec);
}

/*
 * Init Real Time Clock.
 */
void init_rtc()
{
  startup_time = rtc_get_time();
}
