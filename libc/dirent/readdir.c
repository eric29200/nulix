#include <dirent.h>
#include <unistd.h>

/*
 * Read a directory.
 */
struct dirent *readdir(DIR *dirp)
{
  if (dirp == NULL || dirp->fdn < 0)
    return NULL;

  if (read(dirp->fdn, &dirp->dent, sizeof(dirp->dent)) != sizeof(dirp->dent))
    return NULL;

  return &dirp->dent;
}
