#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pwd.h>

int main()
{
	struct passwd *pwd;

	/* get passwd */
	pwd = getpwuid(geteuid());
	if (!pwd)
		exit(EXIT_FAILURE);

	/* print name */
	printf("%s\n", pwd->pw_name);

	return 0;
}
