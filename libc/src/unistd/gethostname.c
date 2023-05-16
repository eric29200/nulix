#include <unistd.h>
#include <sys/utsname.h>

int gethostname(char *name, size_t len)
{
	struct utsname uts;
	size_t i;

	if (uname(&uts))
		return -1;

	if (len > sizeof(uts.nodename))
		len = sizeof(uts.nodename);

	for (i = 0; i < len && uts.nodename[i]; i++)
		name[i] = uts.nodename[i];

	if (i && i == len)
		name[i - 1] = 0;

	return 0;
}
