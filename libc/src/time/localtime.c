#include <errno.h>
#include <limits.h>

#include "__time_impl.h"

static char *time_zones[] = {
	"UTC"
};

static struct tm tm;

struct tm *localtime(const time_t *timep)
{
	/* reject time_t values whose year would overflow int */
	if (*timep < INT_MIN * 31622400LL || *timep > INT_MAX * 31622400LL) {
		errno = EOVERFLOW;
		return NULL;
	}

	/* set default timezone */
	tm.__tm_gmtoff = 0;
	tm.__tm_zone = time_zones[0];

	/* convert seconds to tm */
	if (__secs_to_tm(*timep, &tm) < 0) {
		errno = EOVERFLOW;
		return NULL;
	}

	return &tm;
}