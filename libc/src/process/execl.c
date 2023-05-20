#include <unistd.h>
#include <stdarg.h>

int execl(const char *pathname, const char *arg, ...)
{
	va_list ap;
	int argc;

	va_start(ap, arg);
	for (argc = 1; va_arg(ap, const char *); argc++);
	va_end(ap);

	{
		char *argv[argc + 1];
		int i;

		va_start(ap, arg);
		argv[0] = (char *) arg;
		for (i = 1; i < argc; i++)
			argv[i] = va_arg(ap, char *);
		argv[i] = NULL;
		va_end(ap);


		return execv(pathname, argv);
	}
}