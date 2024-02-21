#include <ctype.h>
#include <string.h>

#include "env_utils.h"
#include "macros.h"

const char *find_rhpenvvar(const char* optname, char **env_varname, size_t *env_varname_max)
{
   const char prefix[] = "RHP_";
   size_t optname_len = strlen(optname), _env_varname_max = *env_varname_max;
   char *_env_varname = *env_varname;

   if (optname_len > _env_varname_max) {
      *env_varname_max = optname_len;
      REALLOC_NULL(*env_varname, char, optname_len + 5);
      _env_varname_max = optname_len;
      _env_varname = *env_varname;
   }

   strncpy(_env_varname, prefix, sizeof prefix+1);
   strncat(_env_varname, optname, _env_varname_max);

   /* ---------------------------------------------------------------------
       * We look for uppercase env variables
       * --------------------------------------------------------------------- */

   char *s = _env_varname;
   while (*s != '\0') {
      *s = toupper(*s);
         s++;
      }

   return mygetenv(_env_varname);
}

