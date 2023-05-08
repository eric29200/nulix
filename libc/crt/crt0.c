#include <unistd.h>
#include <stdlib.h>

#include "../src/stdio/__stdio_impl.h"

extern int main(int, char **, char **);

void _start(int argc, char **argv, char **envp)
{
	int code;

	environ = envp;
	__stdio_init();

	code = main(argc, argv, envp);

	exit(code);
}