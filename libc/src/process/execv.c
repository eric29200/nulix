#include <unistd.h>

int execv(const char *pathname, char *const argv[])
{
	return execve(pathname, argv, environ);
}