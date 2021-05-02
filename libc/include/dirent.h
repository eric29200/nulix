#ifndef _DIRENT_H_
#define _DIRENT_H_

#include <sys/types.h>
#include <limits.h>

/*
 * Directory entry.
 */
struct dirent {
  ino_t d_ino;
  char d_name[FILENAME_MAX_LEN + 1];
};

/*
 * Opened directory.
 */
typedef struct DIR {
  int fdn;
  struct dirent dent;
} DIR;

DIR *opendir(const char *name);
int closedir(DIR *dirp);
struct dirent *readdir(DIR *dirp);

#endif
