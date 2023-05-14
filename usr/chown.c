#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <pwd.h>
#include <grp.h>
#include <sys/stat.h>

/*
 * Get uid from decimal string.
 */
static void __uid_from_decimal(const char *str, uid_t *uid)
{
	*uid = 0;

	while (*str >= '0' && *str <= '9')
		*uid = *uid * 10 + (*str++ - '0');

	if (*str) {
		fprintf(stderr, "Bad uid value\n");
		exit(1);
	}
}

/*
 * Get gid from decimal string.
 */
static void __gid_from_decimal(const char *str, gid_t *gid)
{
	*gid = 0;

	while (*str >= '0' && *str <= '9')
		*gid = *gid * 10 + (*str++ - '0');

	if (*str) {
		fprintf(stderr, "Bad gid value\n");
		exit(1);
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
		exit(1);
	}

	*uid = pwd->pw_uid;
}

/*
 * Get gid from name.
 */
static void __gid_from_name(const char *name, gid_t *gid)
{
	struct group *grp;

	grp = getgrnam(name);
	if (!grp) {
		fprintf(stderr, "%s : unknown group\n", name);
		exit(1);
	}

	*gid = grp->gr_gid;
}

/*
 * Usage.
 */
static void usage(const char *name)
{
	fprintf(stderr, "Usage: %s { user_name | user_id } file [...]\n", name);
}

int main(int argc, char **argv)
{
	struct stat statbuf;
	char *us, *gs;
	uid_t uid;
	gid_t gid;
	int i;

	/* check arguments */
	if (argc < 3) {
		usage(argv[0]);
		exit(1);
	}

	/* get user string */
	us = argv[1];
	if (us[0] == '.' || us[0] == ':')
		us = NULL;

	/* get group string */
	gs = strchr(argv[1], ':');
	if (!gs)
		gs = strchr(argv[1], '.');

	/* end user string */
	if (gs)
		*gs++ = 0;

	/* get uid */
	if (us) {
		if (*us >= '0' && *us <= '9')
			__uid_from_decimal(us, &uid);
		else
			__uid_from_name(us, &uid);
	}
	
	/* get gid */
	if (gs) {
		if (*gs >= '0' && *gs <= '9')
			__gid_from_decimal(gs, &gid);
		else
			__gid_from_name(gs, &gid);
	}

	/* chown arguments */
	for (i = 2; i < argc; i++) {
		/* get uid or gid */
		if (!us || !gs) {
			if (stat(argv[i], &statbuf) < 0) {
				perror(argv[i]);
				continue;
			}

			if (!us)
				uid = statbuf.st_uid;
			if (!gs)
				gid = statbuf.st_gid;
		}
		
		/* chown file */
		if (chown(argv[i], uid, gid) < 0)
			perror(argv[i]);
	}

	return 0;
}
