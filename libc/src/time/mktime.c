#include "__time_impl.h"

time_t mktime(struct tm *tm)
{
	return __tm_to_secs(tm);
}