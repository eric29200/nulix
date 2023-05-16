#include <stdio.h>
#include <unistd.h>

int main()
{
	char hostname[BUFSIZ];

	if (gethostname(hostname, BUFSIZ - 1) < 0) {
		perror("gethostname");
		return 1;
	}

	printf("%s\n", hostname);

	return 0;
}
