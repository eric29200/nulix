#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <pwd.h>
#include <grp.h>
#include <sys/stat.h>

/*
 * Get uid from uid string.
 */
static void __uid_from_uid(const char *uid_str, uid_t *uid)
{
	*uid = 0;

	while (*uid_str >= '0' && *uid_str <= '9')
		*uid = *uid * 10 + (*uid_str++ - '0');

	if (*uid_str) {
		fprintf(stderr, "Bad uid value\n");
		exit(EXIT_FAILURE);
	}
}

/*
 * Get uid from name.
 */
static void __uid_from_name(const char *name, uid_t *uid)
{
	struct passwd *pwd;

	pwd = getpwnam(name);
	if (!pwd) {
		fprintf(stderr, "%s : unknown user\n", name);
		exit(EXIT_FAILURE);
	}

	*uid = pwd->pw_uid;
}

int main(int argc, char **argv)
{
	struct stat statbuf;
	uid_t uid;
	char *s;
	int i;

	/* check arguments */
	if (argc < 3) {
		fprintf(stderr, "Usage: %s { user_name | user_id } file [...]\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	/* get uid */
	s = argv[1];
	if (*s >= '0' && *s <= '9')
		__uid_from_uid(s, &uid);
	else
		__uid_from_name(s, &uid);

	/* chown arguments */
	for (i = 2; i < argc; i++)
		if (stat(argv[i], &statbuf) < 0 || chown(argv[i], uid, statbuf.st_gid) < 0)
			perror(argv[i]);

	return 0;
}