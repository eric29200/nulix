#include <string.h>

#include "__time_impl.h"

static char *weekdays[] = {
	"Sunday",
	"Monday",
	"Tuesday",
	"Wednesday",
	"Thursday",
	"Friday",
	"Saturday",
};

static char *weekdays_short[] = {
	"Sun",
	"Mon",
	"Tue",
	"Wed",
	"Thu",
	"Fri",
	"Sat"
};

static char *months[] = {
	"January",
	"February",
	"March",
	"April",
	"May",
	"June",
	"July",
	"August",
	"September",
	"October",
	"November",
	"December"
};

static char *months_short[] = {
	"Jan",
	"Feb",
	"Mar",
	"Apr",
	"May",
	"Jun",
	"Jul",
	"Aug",
	"Sep",
	"Oct",
	"Nov",
	"Dec"
};

/*
 * Format one field.
 */
static char *__strftime_1(char (*buf)[100], size_t *len, int format, const struct tm *tm)
{
	switch (format) {
		case 'a':
			*len = snprintf(*buf, sizeof(*buf), "%s", weekdays_short[tm->tm_wday]);
			break;
		case 'A':
			*len = snprintf(*buf, sizeof(*buf), "%s", weekdays[tm->tm_wday]);
			break;
		case 'h':
		case 'b':
			*len = snprintf(*buf, sizeof(*buf), "%s", months_short[tm->tm_mon]);
			break;
		case 'B':
			*len = snprintf(*buf, sizeof(*buf), "%s", months[tm->tm_mon]);
			break;
		case 'c':
			*len = snprintf(*buf, sizeof(*buf), "%s %s %02d %02d:%02d:%02d %04d",
					weekdays_short[tm->tm_wday],
					months_short[tm->tm_mon],
					tm->tm_mday,
					tm->tm_hour,
					tm->tm_min,
					tm->tm_sec,
					tm->tm_year + 1900);
			break;
		case 'C':
			*len = snprintf(*buf, sizeof(*buf), "%02d", (tm->tm_year + 1900) / 100);
			break;
		case 'd':
			*len = snprintf(*buf, sizeof(*buf), "%02d", tm->tm_mday);
			break;
		case 'D':
			*len = snprintf(*buf, sizeof(*buf), "%02d/%02d/%02d", tm->tm_mon + 1, tm->tm_mday, tm->tm_year % 100);
			break;
		case 'e':
			*len = snprintf(*buf, sizeof(*buf), "%2d", tm->tm_mday);
			break;
		case 'F':
			*len = snprintf(*buf, sizeof(*buf), "%04d-%02d-%02d", tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday);
			break;
		case 'H':
			*len = snprintf(*buf, sizeof(*buf), "%02d", tm->tm_hour);
			break;
		case 'I':
			*len = snprintf(*buf, sizeof(*buf), "%02d", tm->tm_hour == 0 ? 12 : (tm->tm_hour == 12 ? 12 : (tm->tm_hour % 12)));
			break;
		case 'j':
			*len = snprintf(*buf, sizeof(*buf), "%03d", tm->tm_yday);
			break;
		case 'k':
			*len = snprintf(*buf, sizeof(*buf), "%2d", tm->tm_hour);
			break;
		case 'l':
			*len = snprintf(*buf, sizeof(*buf), "%2d", tm->tm_hour == 0 ? 12 : (tm->tm_hour == 12 ? 12 : (tm->tm_hour % 12)));
			break;
		case 'm':
			*len = snprintf(*buf, sizeof(*buf), "%02d", tm->tm_mon + 1);
			break;
		case 'M':
			*len = snprintf(*buf, sizeof(*buf), "%02d", tm->tm_min);
			break;
		case 'n':
			*len = snprintf(*buf, sizeof(*buf), "\n");
			break;
		case 'p':
			*len = snprintf(*buf, sizeof(*buf), "%s", tm->tm_hour < 12 ? "AM" : "PM");
			break;
		case 'P':
			*len = snprintf(*buf, sizeof(*buf), "%s", tm->tm_hour < 12 ? "am" : "pm");
			break;
		case 'r':
			*len = snprintf(*buf, sizeof(*buf), "%02d:%02d:%02d %s",
					tm->tm_hour == 0 ? 12 : (tm->tm_hour == 12 ? 12 : (tm->tm_hour % 12)),
					tm->tm_min,
					tm->tm_sec,
					tm->tm_hour < 12 ? "AM" : "PM");
			break;
		case 'R':
			*len = snprintf(*buf, sizeof(*buf), "%02d:%02d", tm->tm_hour, tm->tm_min);
			break;
		case 's':
			*len = snprintf(*buf, sizeof(*buf), "%ld", mktime((struct tm *) tm));
			break;
		case 'S':
			*len = snprintf(*buf, sizeof(*buf), "%02d", tm->tm_sec);
			break;
		case 't':
			*len = snprintf(*buf, sizeof(*buf), "\t");
			break;
		case 'T':
			*len = snprintf(*buf, sizeof(*buf), "%02d:%02d:%02d", tm->tm_hour, tm->tm_min, tm->tm_sec);
			break;
		case 'u':
			*len = snprintf(*buf, sizeof(*buf), "%d", tm->tm_wday == 0 ? 7 : tm->tm_wday);
			break;
		case 'w':
			*len = snprintf(*buf, sizeof(*buf), "%d", tm->tm_wday);
			break;
		case 'x':
			*len = snprintf(*buf, sizeof(*buf), "%02d/%02d/%02d", tm->tm_mon + 1, tm->tm_mday, tm->tm_year % 100);
			break;
		case 'X':
			*len = snprintf(*buf, sizeof(*buf), "%02d:%02d:%02d", tm->tm_hour, tm->tm_min, tm->tm_sec);
			break;
		case 'y':
			*len = snprintf(*buf, sizeof(*buf), "%02d", tm->tm_year % 100);
			break;
		case 'Y':
			*len = snprintf(*buf, sizeof(*buf), "%04d", tm->tm_year + 1900);
			break;
		case 'z':
		 	if (tm->__tm_gmtoff)
				*len = snprintf(*buf, sizeof(*buf), "+%04d", tm->__tm_gmtoff);
			else
				*len = snprintf(*buf, sizeof(*buf), "-%04d", -tm->__tm_gmtoff);
			break;
		case 'Z':
			*len = snprintf(*buf, sizeof(*buf), tm->__tm_zone);
			break;
		case '%':
			*len = snprintf(*buf, sizeof(*buf), "%c", '%');
			break;
		default:
			return NULL;
	}

	return *buf;
}


size_t strftime(char *str, size_t max, const char *format, const struct tm *tm)
{
	char buf[100], *b;
	size_t l, k;

	for (l = 0; l < max; format++) {
		/* end format */
		if (!*format) {
			str[l] = 0;
			return l;
		}

		/* litteral */
		if (*format != '%') {
			str[l++] = *format;
			continue;
		}

		/* get next format characer */
		format++;

		/* skip E or 0 */
		if (*format == 'E' || *format == '0') {
			format++;
		}

		/* format in temporary buffer */
		b = __strftime_1(&buf, &k, *format, tm);
		if (!b)
			break;

		/* limit to remaining length */
		if (k > max - l)
			k = max - l;

		/* update written length */
		memcpy(str + l, b, k);
		l += k;
	}

	/* end string */
	if (max) {
		if (l == max)
			l = max - 1;

		str[l] = 0;
	}

	return 0;
}