#include "asprintf.h"

#include <stdio.h>
#include <stdlib.h>

#include "macros.h"
#include "printout.h"
#include "sys_utils.h"


/**
 * @brief Trim the newline in a string
 *
 * @param[in,out] str the string
 * @param         len the length of the string 
 */
void trim_newline(char *str, size_t len)
{
   assert(strlen(str) == len);

   /* Trim newlines */
   while (len > 0 && (str[len-1] == '\n' || str[len-1] == '\r')) { len--; }

   str[len] = '\0';
}

const char* exe_getfullpath(const char *executable)
{
#if defined(_POSIX_C_SOURCE) && _POSIX_C_SOURCE >= 200809L
   char *cmd;

   IO_CALL_NULL(asprintf(&cmd, "which %s", executable));

   // Execute the command using popen and read the output
   FILE *pipe = popen(cmd, "r");

   if (!pipe) {
      FREE(cmd);
      return NULL;
   }
      
   char path[256];
   char *res = fgets(path, sizeof(path), pipe);

   if (!res) { 
      pclose(pipe);
      return NULL;
   }

   size_t read = 0;
   do {

      size_t pathlen = strlen(path);
      REALLOC_NULL(cmd, char, read+pathlen+1);

      memcpy(cmd + read, path, pathlen + 1);
      read += pathlen;

   }  while(fgets(path, sizeof(path), pipe));

   trim_newline(cmd, read);

   return cmd;
#endif

   return NULL;
}
