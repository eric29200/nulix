#include <stdio.h>
#include <sys/utsname.h>

#define FLAG_SYSNAME		(1 << 0)
#define FLAG_NODENAME		(1 << 1)
#define FLAG_RELEASE		(1 << 2)
#define FLAG_VERSION		(1 << 3)
#define FLAG_MACHINE		(1 << 4)

int main(int argc, char **argv)
{
	struct utsname buf;
	int flags = 0, i;
	char *s;

	/* parse arguments */
	for (i = 1; i < argc; i++) {
		s = argv[i];
		
		if (*s++ != '-')
			continue;

		while (*s) {
			switch (*s++) {
				case 's':
					flags |= FLAG_SYSNAME;
					break;
				case 'n':
					flags |= FLAG_NODENAME;
					break;
				case 'r':
					flags |= FLAG_RELEASE;
					break;
				case 'v':
					flags |= FLAG_VERSION;
					break;
				case 'm':
					flags |= FLAG_MACHINE;
					break;
				case 'a':
					flags |= FLAG_SYSNAME | FLAG_NODENAME | FLAG_RELEASE | FLAG_VERSION | FLAG_MACHINE;
					break;
				default:
					break;
			}
		}
	}

	/* default flag */
	if (!flags)
		flags = FLAG_SYSNAME;

	/* print uname */
	if (uname(&buf) == 0) {
		if (flags & FLAG_SYSNAME)
			printf("%s ", buf.sysname);
		if (flags & FLAG_NODENAME)
			printf("%s ", buf.nodename);
		if (flags & FLAG_RELEASE)
			printf("%s ", buf.release);
		if (flags & FLAG_VERSION)
			printf("%s ", buf.version);
		if (flags & FLAG_MACHINE)
			printf("%s ", buf.machine);
		printf("\n");
	}

	return 0;
}