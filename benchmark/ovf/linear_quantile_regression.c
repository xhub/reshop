#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "bench_utils.h"
#include "reshop.h"
#include "test_ovf.h"
#include "arg_cauldron.h"
#include "compat.h"

static void str2uint(const char *str, unsigned *s, const char *argname)
{
   char * endptr;
   errno = 0;
   long val = strtol(str, &endptr, 10);

   /* Check for various possible errors. */

   if (errno != 0) {
      perror("strtol");
      exit(EXIT_FAILURE);
   }

   if (endptr == str) {
      fprintf(stderr, "No digits were found\n");
      exit(EXIT_FAILURE);
   }

   /* If we got here, strtol() successfully parsed a number. */

   if (*endptr != '\0') {        /* Not necessarily an error... */
      printf("Further characters after number: \"%s\"\n", endptr);
   }

   if (val < 0) {
      fprintf(stderr, "argument %s must be non-negative!\n", argname);
      exit(EXIT_FAILURE);
   }

   *s = val;
}

int main(UNUSED int argc, char **argv)
{

   unsigned s_default = 10000;
   unsigned s = s_default;
   char  *startptr;

   char *argv0 = argv[0];
	char const *formulation = "equilibrium", *solvername = "";
   UNUSED bool init_var = false;

	ARG_BEGIN {
		if (0) { 
		} else if (ARG_LONG("init_vars")) case 'i': {
			init_var = true;
			ARG_FLAG();
		} else if (ARG_LONG("solver")) case 's': {
			solvername = ARG_VAL();
		} else if (ARG_LONG("formulation")) case 'f': {
			formulation = ARG_VAL();
		} else if (ARG_LONG("size")) case 'S': {
			startptr = ARG_VAL();
         str2uint(startptr, &s, "size");
		} else if (ARG_LONG("help")) case 'h': case '?': {
			printf("Usage: %s [OPTION...] [STRING...]\n", argv0);
			puts("Example usage of arg.h\n");
			puts("Options:");
		   puts("  -i, --init_vars         initialize new variables");
			puts("  -f, --formulation=STR   set formulation for OVF");
			printf("  -S, --size=NUM          set sample size (default is %u)\n", s_default);
			puts("  -s, --solver=STR        set solver");
			puts("  -h, --help              display this help and exit");
			return EXIT_SUCCESS;
		} else {FALLTHRU default:
			(void)fprintf(stderr,
			        "%s: invalid option '%s'\n"
			        "Try '%s --help' for more information.\n",
			        argv0, *argv, argv0);
			return EXIT_FAILURE;
		}
	} ARG_END;

  /* ----------------------------------------------------------------------
   * If mdl_solver is NULL, we could just be skipping a solver
   * ---------------------------------------------------------------------- */

   struct rhp_mdl *mdl_solver = bench_getgamssolver(solvername);
   if (!mdl_solver) { return 0; }

   struct rhp_mdl *mdl = bench_init(1e-6);

   double xsols_dummy[] = {NAN, NAN, NAN};

   int status = linear_quantile_regression(mdl, mdl_solver, formulation, s, xsols_dummy);

   rhp_mdl_free(mdl_solver);
   rhp_mdl_free(mdl);

   return status;
}
