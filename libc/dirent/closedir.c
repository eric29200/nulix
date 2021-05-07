#include <dirent.h>
#include <unistd.h>
#include <stdlib.h>

/*
 * Close a directory.
 */
int closedir(DIR *dir)
{
  int ret = close(dir->fd);
  free(dir);
  return ret;
}
