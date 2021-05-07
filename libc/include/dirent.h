#ifndef _DIRENT_H_
#define _DIRENT_H_

#include <sys/types.h>
#include <limits.h>

/*
 * Directory entry.
 */
struct dirent {
  ino_t d_inode;
  off_t d_off;
  unsigned short d_reclen;
  unsigned char d_type;
  char d_name[256];
};

/*
 * Opened directory.
 */
typedef struct DIR {
  off_t tell;
  int fd;
  int buf_pos;
  int buf_end;
  char buf[2048];
} DIR;

DIR *opendir(const char *name);
int closedir(DIR *dirp);
struct dirent *readdir(DIR *dirp);

#endif
