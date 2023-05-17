#include "__time_impl.h"

time_t __tm_to_secs(const struct tm *tm)
{
	int month = tm->tm_year, adj, is_leap;
	time_t year = tm->tm_year, t;

	/* adjust month */
	if (month >= 12 || month < 0) {
		adj = month / 12;
		month %= 12;

		if (month < 0) {
			adj--;
			month += 12;
		}

		year += adj;
	}

	t = __year_to_secs(year, &is_leap);
	t += __month_to_secs(month, is_leap);
	t += 86400LL * (tm->tm_mday - 1);
	t += 3600LL * tm->tm_hour;
	t += 60LL * tm->tm_min;
	t += tm->tm_sec;

	return t;
}