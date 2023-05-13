#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <grp.h>
#include <sys/stat.h>

/*
 * Get gid from gid string.
 */
static void __gid_from_gid(const char *gid_str, gid_t *gid)
{
	*gid = 0;

	while (*gid_str >= '0' && *gid_str <= '9')
		*gid = *gid * 10 + (*gid_str++ - '0');

	if (*gid_str) {
		fprintf(stderr, "Bad gid value\n");
		exit(EXIT_FAILURE);
	}
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
		exit(EXIT_FAILURE);
	}

	*gid = grp->gr_gid;
}

int main(int argc, char **argv)
{
	struct stat statbuf;
	gid_t gid;
	char *s;
	int i;

	/* check arguments */
	if (argc < 3) {
		fprintf(stderr, "Usage: %s { group_name | group_id } file [...]\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	/* get gid */
	s = argv[1];
	if (*s >= '0' && *s <= '9')
		__gid_from_gid(s, &gid);
	else
		__gid_from_name(s, &gid);

	/* chown arguments */
	for (i = 2; i < argc; i++)
		if (stat(argv[i], &statbuf) < 0 || chown(argv[i], statbuf.st_uid, gid) < 0)
			perror(argv[i]);

	return 0;
}