#include <time.h>

/*
 * Convert Gregorian date to seconds since 1970-01-01 00:00.
 */
time_t mktime(uint32_t year, uint32_t month, int32_t day, uint32_t hour, uint32_t min, uint32_t sec)
{
	/* leap years */
	if (0 >= (int) (month -= 2)) {
		month += 12;
		year -= 1;
	}

return ((((unsigned long) (year / 4 - year / 100 + year / 400 + 367 * month / 12 + day) + year * 365 - 719499
	  ) * 24 + hour		/* now have hours */
	 ) * 60 + min		/* now have minutes */
	) * 60 + sec;		/* finally seconds */
}
