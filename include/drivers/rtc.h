#ifndef _RTC_H_
#define _RTC_H_

#include <stddef.h>

#define CMOS_ADDRESS        0x70
#define CMOS_DATA           0x71

#define RTC_FREQ_SELECT     0x0A
#define RTC_CONTROL         0x0B

#define RTC_UIP             0x80    /* update in progress flag */
#define RTC_DM_BINARY       0x04    /* all time/date values are BCD if clear */
#define RTC_ALWAYS_BCD      1       /* RTC operates in binary mode */

#define RTC_SECOND          0x00
#define RTC_MINUTE          0x02
#define RTC_HOUR            0x04
#define RTC_DAY             0x07
#define RTC_MONTH           0x08
#define RTC_YEAR            0x09

#define CMOS_READ(addr)     ({outb(CMOS_ADDRESS, addr); inb(CMOS_DATA);})
#define BCD_TO_BIN(val)     ((val) = ((val) & 15) + ((val) >> 4) * 10)

void init_rtc();

#endif
