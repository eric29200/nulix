#include <pwd.h>

#include "__passwd_impl.h"

static char *line = NULL;
static struct passwd pw;
static size_t size;

struct passwd *getpwuid(uid_t uid)
{
	struct passwd *res;
	__getpw(NULL, uid, &pw, &line, &size, &res);
	return res;
}

struct passwd *getpwnam(const char *name)
{
	struct passwd *res;
	__getpw(name, 0, &pw, &line, &size, &res);
	return res;
}