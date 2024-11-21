#include "embcode_empinfo.h"
#include "macros.h"
#include "printout.h"
#include "reshop-gams.h"
#include "reshop.h"
#include "status.h"


int rhp_embcode(void *gmd, unsigned char scrdirlen, const char *scrdir, unsigned char codelen, const char *code, unsigned char argslen, const char *args)
{
   /* Print the reshop banner */
   rhp_print_banner();

   trace_empinterp("[embcode] scrdir '%.*s', file '%.*s' and args '%.*s'\n", scrdirlen, scrdir, codelen, code, argslen, args);
   int status = OK;

   if (argslen > 0) {
      error("[embCode] ERROR: argument '%*s' not yet supported\n", argslen, args);
      return Error_NotImplemented;
   }

   char *fname = NULL, *scrdir2;
   MALLOC_(fname, char, codelen+1);
   MALLOC_EXIT(scrdir2, char, scrdirlen+1);

   strncpy(fname, code, codelen);
   fname[codelen] = '\0';
   strncpy(scrdir2, scrdir, scrdirlen);
   scrdir2[scrdirlen] = '\0';

   S_CHECK_EXIT(embcode_process_empinfo(gmd, scrdir2, fname))

_exit:
   free(fname);
   free(scrdir2);

   return status;
}
