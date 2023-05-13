#include <stdio.h>

int main(int argc, char **argv)
{
	char *yes = "y";

	if (argc > 1)
		yes = argv[1];

	for (;;)
		printf("%s\n", yes);

	return 0;
}
