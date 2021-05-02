#include <dirent.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/*
 * Open a directory.
 */
DIR *opendir(const char *name)
{
  struct stat statbuf;
  DIR *dirp;
  int fdn;

  /* stat directory */
  if (stat(name, &statbuf) != 0)
    return NULL;
  /* check directory */
  if (!S_ISDIR(statbuf.st_mode))
    return NULL;

  /* open directory */
  fdn = open(name);
  if (fdn < 0)
    return NULL;

  dirp = (DIR *) malloc(sizeof(DIR));
  if (dirp == NULL) {
    close(fdn);
    return NULL;
  }

  dirp->fdn = fdn;
  memset(&dirp->dent, 0, sizeof(dirp->dent));

  return dirp;
}
