#include <ctype.h>
#include <string.h>

#include "env_utils.h"
#include "macros.h"
#include "string_utils.h"

/**
 * @brief Find the envirronment variable "RHP_<optname>"
 *
 * @param         optname           the option name
 * @param[in,out] env_varname       the buffer for the envirronment variable
 * @param[in,out] env_varname_max   the max length of env_varname (on both input and output)
 *
 * @return                          the value of RHP_<optname>
 */
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

   char * restrict s = _env_varname;
   while (*s) { *s = RhpToUpper(*s); s++; }

   return mygetenv(_env_varname);
}

