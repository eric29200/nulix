#include <drivers/char/rtc.h>
#include <x86/io.h>
#include <time.h>

uint32_t startup_time = 0;

/*
 * Read a value from a CMOS register.
 */
static uint8_t cmos_read(uint8_t addr)
{
	outb(CMOS_ADDRESS, addr);
	return inb(CMOS_DATA);
}

/*
 * Get real time clock.
 */
static uint32_t rtc_get_time()
{
	uint32_t year, month, day, hour, min, sec;
	int i;

	/* wait for the UIP flag (Update In Progress) to go from 1 to 0 = the RTC registers should hold the precise second */
	for (i = 0; i < 1000000; i++)
		if (cmos_read(RTC_FREQ_SELECT) & RTC_UIP)
			break;

	for (i = 0; i < 1000000; i++)
		if (!(cmos_read(RTC_FREQ_SELECT) & RTC_UIP))
			break;

	/* read time from cmos */
	do {
		sec = cmos_read(RTC_SECOND);
		min = cmos_read(RTC_MINUTE);
		hour = cmos_read(RTC_HOUR);
		day = cmos_read(RTC_DAY);
		month = cmos_read(RTC_MONTH);
		year = cmos_read(RTC_YEAR);
	} while (sec != cmos_read(RTC_SECOND));

	/* convert BCD time to BIN time if needed */
	if (!(cmos_read(RTC_CONTROL) & RTC_DM_BINARY) || RTC_ALWAYS_BCD) {
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
