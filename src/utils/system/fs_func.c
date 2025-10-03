#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>

#ifdef _WIN32
#include <io.h>
#define F_OK 0
#define R_OK 4
#define access _access

#else /* _WIN32 */
#include <unistd.h>
#endif


#include "fs_func.h"
#include "printout.h"
#include "status.h"

/* Little GLIBC fiasco: with version 2.33, stat no longer defined
 * as a macro, but as a true function. Before that, it
 * would be define as a call to __xstat. However, the latter take
 * 3 arguments, the first one being the version of the stat struct.
 * 
 * So the classical trick of using the old symbol does not work ...*/
#ifdef __linux__
#include <dirent.h>
bool dir_exists(const char *dirname)
{
   DIR *dir = opendir(dirname);

   bool res = dir ? true : false;

   if (dir) {
      closedir(dir);
   }

   return res;
}

#else

#include <sys/stat.h>

#ifdef RHP_COMPILER_CL
#define stat _stat
#endif

bool dir_exists(const char *dirname)
{
   struct stat info;
   int rc = stat(dirname, &info);
   return rc == 0;
}
#endif

/**
 * @brief Create a new directory name, adding a number at the end if needed
 *
 * @param  newdir      string containing the name of the seried directory
 * @param  newdir_len  length of the newdir string
 *
 * @return             the error code
 */
int new_unique_dirname(char *newdir, unsigned newdir_len)
{
   /* This is a bit too simplistic, it may fail for many reason --xhub */
   size_t len1 = strlen(newdir);
   unsigned i = 0;

   do {
      if (dir_exists(newdir)) {
         if (len1 < newdir_len - 13) {
            snprintf(&newdir[len1], newdir_len - len1 + 1, "_%u", i);
         } else {
            snprintf(&newdir[newdir_len - 13], 12, "_%u", i);
         }
      } else {
         break;
      }

      ++i;
      if (i == UINT_MAX) {
         newdir[len1] = '\0';
         error("%s :: No unique new directoryname based on %s could be created."
               " Check that the parent directly exists, or delete the existing "
               "directories in the parent one\n", __func__, newdir);
         return Error_SystemError;
      }
   } while (true);

   return OK;
}

bool file_readable_silent(const char *fname)
{
   return !access(fname, R_OK);
}

int file_readable(const char *fname)
{
   if (!access(fname, R_OK)) { return 0; }

   if (!access(fname, F_OK)) {
      error("ERROR! Cannot read (permission issue) file '%s'\n", fname);
   } else {
      error("ERROR! File '%s' does not exists\n", fname);
   }

   return Error_FileOpenFailed;
}
