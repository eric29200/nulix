#ifndef _LIBC_PWD_IMPL_H_
#define _LIBC_PWD_IMPL_H_

#include <pwd.h>
#include <grp.h>

int __getpwent(FILE *fp, struct passwd *pw, char **line, size_t *size, struct passwd **res);
int __getpw(const char *name, uid_t uid, struct passwd *pw, char **buf, size_t *size, struct passwd **res);

int __getgrent(FILE *fp, struct group *gr, char **line, size_t *size, char ***mem, size_t *nrmem, struct group **res);
int __getgr(const char *name, gid_t gid, struct group *gr, char **buf, size_t *size, char ***mem, size_t *nrmem, struct group **res);

#endif