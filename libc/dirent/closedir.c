#include <dirent.h>
#include <unistd.h>
#include <stdlib.h>

/*
 * Close a directory.
 */
int closedir(DIR *dirp)
{
  int ret;

  if (dirp == NULL || dirp->fdn < 0)
    return -1;

  ret = close(dirp->fdn);
  free(dirp);
  return ret;
}
