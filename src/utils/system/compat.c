#include "reshop_config.h"
#include "compat.h"

#ifdef _WIN32

#include <windows.h>
#include <tchar.h>
#include <shellapi.h>

/**
 * @brief Recursively remove a directory
 *
 * @param pathname the directory to remove
 *
 * @return the error code 
 */
int rmfn(const char *pathname)
{
  size_t len = strlen(pathname);
  char *path00 = (char*)malloc(len+2*sizeof(char));
  memcpy(path00, pathname, len+1);
  path00[len+1] = '\0';

  SHFILEOPSTRUCT fileop;
  fileop.hwnd   = NULL;    // no status display
  fileop.wFunc  = FO_DELETE;
  fileop.pFrom  = path00;
  fileop.pTo    = NULL;
  fileop.fFlags = FOF_NOCONFIRMATION | FOF_SILENT;  // do not prompt the user

  fileop.fAnyOperationsAborted = FALSE;
  fileop.lpszProgressTitle     = NULL;
  fileop.hNameMappings         = NULL;

  int rc = SHFileOperation(&fileop);
  return rc;

}

#else


#include <ftw.h>
#include <stdio.h>
#include <unistd.h>

#include "printout.h"
#include "status.h"

static int _rmelt(const char *fpath, const struct stat *sb, int tflag,
              struct FTW *ftwbuf)
{
   if (tflag == FTW_DP) {
      if(rmdir(fpath) == -1) {
         perror("rmdir");
         return 1;
      }
   } else {
      if (unlink(fpath) == -1) {
         perror("unlink");
         return 1;
      }
   }

   return 0;
}

/**
 * @brief Recursively remove a directory
 *
 * @param pathname the directory to remove
 *
 * @return the error code 
 */
int rmfn(const char *pathname)
{
   if (nftw(pathname, _rmelt, FOPEN_MAX, FTW_DEPTH | FTW_PHYS) == -1) {
      perror("nftw");
      error("%s :: remove of the path %s failed!\n", __func__,
               pathname);
      return Error_SystemError;
   }

   return OK;
}

#endif

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <processenv.h>

#include "macros.h"
#include "printout.h"

const char * mygetenv(const char *envvar)
{
   DWORD bufsize = 4096;
   char *envval;
   MALLOC_NULL(envval, char, bufsize);

   DWORD ret = GetEnvironmentVariableA(envvar, envval, bufsize);

   if (ret == 0) {
      FREE(envval);
      return NULL;
   }

   if (ret > bufsize) {
      bufsize = ret;
      REALLOC_NULL(envval, char, bufsize);
      ret = GetEnvironmentVariableA(envvar, envval, bufsize);

      if (!ret) { FREE(envval); return NULL; }
   }

   return envval;
}

void myfreeenvval(const char *envval)
{
   if (envval) { free((char*)envval); }
}
#endif
