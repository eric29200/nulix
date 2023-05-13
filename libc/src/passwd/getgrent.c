#include <grp.h>

#include "__passwd_impl.h"

static char *line = NULL;
static char **mem = NULL;
static struct group gr;

struct group *getgrgid(gid_t gid)
{
	size_t size = 0, nrmem = 0;
	struct group *res;

	__getgr(NULL, gid, &gr, &line, &size, &mem, &nrmem, &res);

	return res;
}

struct group *getgrnam(const char *name)
{
	size_t size = 0, nrmem = 0;
	struct group *res;
	
	__getgr(name, 0, &gr, &line, &size, &mem, &nrmem, &res);

	return res;
}