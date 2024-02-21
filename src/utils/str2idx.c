#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#include "compat.h"
#include "printout.h"
#include "status.h"
#include "str2idx.h"

/**
 * @brief Find the index of string in an array of string
 *
 * @param strs  the array of strings
 * @param name  the name
 *
 * @return      the index if the string was found in the array, or SIZE_MAX
 */
size_t find_str_idx(const char * const * const strs, const char *name)
{
   assert(strs);
   size_t i = 0;

   while(strs[i]) {
      if (!strcasecmp(name, strs[i])) {
         return i;
      }
      i++;
    }

   error("ERROR: no string named '%s found\n\tThe available ones are:", name);
   i = 0;
   while(strs[i]) {
     error(" %s", strs[i]);
     i++;
   }

   errormsg("\n");

   return SIZE_MAX;
}
