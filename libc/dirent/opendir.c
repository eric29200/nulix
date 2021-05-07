#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

/*
 * Open a directory.
 */
DIR *opendir(const char *name)
{
  struct stat statbuf;
  DIR *dir;
  int fd;

  /* stat directory */
  if (stat(name, &statbuf) != 0)
    return NULL;

  /* check directory */
  if (!S_ISDIR(statbuf.st_mode))
    return NULL;

  /* open directory */
  fd = open(name, O_RDONLY, 0);
  if (fd < 0)
    return NULL;

  dir = (DIR *) malloc(sizeof(DIR));
  if (dir == NULL) {
    close(fd);
    return NULL;
  }

  dir->fd = fd;
  return dir;
}
