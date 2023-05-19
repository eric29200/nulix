#include <unistd.h>
#include <stdlib.h>

#include "../src/stdio/__stdio_impl.h"

#define START "_start"

#include "crt0.h"

extern int main(int, char **, char **);

void _start_c(long *p)
{
	char **argv, **envp;
	int argc, code;

	/* get argc, argv and envp */
	argc = p[0];
	argv = (void *) (p + 1);
	envp = argv + argc + 1;

	/* init stdio */
	environ = envp;
	__stdio_init();

	/* execute main */
	code = main(argc, argv, envp);

	/* exit */
	exit(code);
}
