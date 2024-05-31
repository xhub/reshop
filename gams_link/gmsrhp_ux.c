/* simple main program that loads a GAMS control file and
 * does some inspection on the model instance
 */

#include <stdio.h>
#include <stdlib.h>

#include "gmomcc.h"
#include "gevmcc.h"
#include "reshop.h"

int main(int argc, char** argv)
{
   struct rhp_mdl *mdl = NULL;
   int rc = EXIT_FAILURE;

   if( argc < 2 )
   {
      printf("usage: %s <cntrlfile>\n", argv[0]);
      return 1;
   }

   mdl = rhp_gms_newfromcntr(argv[1]);

   if (!mdl) {
      fprintf(stderr, "*** ReSHOP ERROR! Could not load control file %s\n", argv[1]);
      goto TERMINATE;
   }

   rc = EXIT_SUCCESS;

TERMINATE:

   if (mdl) { rhp_mdl_free(mdl); }

   return rc;
}

