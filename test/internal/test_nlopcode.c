
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "dot_png.h"
#include "gams_nlcode.h"
#include "gams_nlcode_priv.h"
#include "rhp_defines.h"
#include "test_instr.h"

const int * const ex_opcodes[] = {
      ex1_opcodes, ex2_opcodes, ex3_opcodes, ex4_opcodes, ex5_opcodes,
      ex6_opcodes, ex7_opcodes, ex8_opcodes,
};

const int* const ex_args[] = {
      ex1_args, ex2_args, ex3_args, ex4_args, ex5_args,
      ex6_args, ex7_args, ex8_args,
   };

const unsigned ex_sizes[] = {
      sizeof(ex1_opcodes), sizeof(ex2_opcodes), sizeof(ex3_opcodes),
      sizeof(ex4_opcodes), sizeof(ex5_opcodes), sizeof(ex6_opcodes),
      sizeof(ex7_opcodes), sizeof(ex8_opcodes),
   };

#define ARRAY_SIZE(arr) (sizeof(arr)/sizeof((arr)[0]))

#define OPCODES(x) (x ## _opcodes)
#define OPARGS(x)  (x ## _args)
#define DEGREE(x)  (x ## _degree)

#define LIN_NLOPCODES() \
DEF(lin1)  \
DEF(lin2a) \
DEF(lin2b) \
DEF(lin3)


#define EX_NLOPCODES() \
DEF(ex1)  \
DEF(ex2)  \
DEF(ex3)  \
DEF(ex4)  \
DEF(ex5)  \
DEF(ex6)  \
DEF(ex7)  \
DEF(ex8)  \



#define _CONCAT(x,y)  x ## y

#define DEF(x) OPCODES(x) , 

const int * const lin_opcodes[] = {
LIN_NLOPCODES()
};

#undef DEF
#define DEF(x) OPARGS(x) ,

const int * const lin_args[] = {
LIN_NLOPCODES()
};
#undef DEF

#define TESTS_NLOPCODES() \
LIN_NLOPCODES() \
EX_NLOPCODES()


#define CHK_SIZES(instrs, args) RESHOP_STATIC_ASSERT(sizeof(args) == sizeof(instrs), "error in opcodes")
#define DEF(x) CHK_SIZES(OPCODES(x), OPARGS(x))
TESTS_NLOPCODES()
#undef DEF


#ifdef QUICK_TEST_LINEARITY
static int test_linearity(const int *instrs, const int *args, bool isLinear)
{
   return 0;
}
#endif

static int test_compute_degree(const int *instrs, const int *args, u8 degree, const char *testname)
{
   u8 degree_computed = compute_nlcode_degree(args[0], instrs, args);

   if (degree_computed != degree) {
      (void)fprintf(stderr, "ERROR: expected degree of %s: \t%u, got \t%u\n", testname, degree, degree_computed);
      return 1;
   }

   return 0;
}

static int test_nlopcode2dot(const int *instrs, const int *args, const char *testname)
{
   char *fname_dot = NULL;
   int rc = gams_nlcode2dot(NULL, instrs, args, &fname_dot);

   if (rc) {
      (void)fprintf(stderr, "ERROR while create DOT representation of %s\n", testname);
      goto exit_;
   }

   char *do_display = mygetenv("RHP_TEST_NLOPCODE_DISPLAY");
   
   if (do_display) {
      if ((rc = dot2png(fname_dot))) { goto exit_; }
      if ((rc = view_png(fname_dot, NULL))) { goto exit_; }
   }

exit_:
   myfreeenvval(do_display);

   return rc;
}

static int test_nlopcode2otree(const int *instrs, const int *args, const char *testname)
{
   char *fname_dot = NULL;
   int rc = 0;

   GamsOpCodeTree* otree = gams_opcodetree_new(args[0], instrs, args);

   if (!otree) {
      (void)fprintf(stderr, "ERROR while create opcode tree of %s\n", testname);
   }

   char *do_display = mygetenv("RHP_TEST_NLOPCODE_DISPLAY");
 
   if (do_display) {
      rc = gams_opcodetree2dot(NULL, otree, &fname_dot);

      if (rc) {
         (void)fprintf(stderr, "ERROR while create DOT representation of %s\n", testname);
         goto exit_;
      }

      if ((rc = dot2png(fname_dot))) { goto exit_; }
      if ((rc = view_png(fname_dot, NULL))) { goto exit_; }
   }

   int *instrs2, *args2, len = args[0];
   rc = gams_otree2instrs(NULL, otree, &instrs2, &args2);

   bool idiff = memcmp(instrs, instrs2, len);
   bool adiff = memcmp(args, args2, len);
   if (idiff) {
      (void)fprintf(stderr, "ERROR: differences in instructions for test %s\n", testname);
      for (u32 i = 0; i < len; ++i) {
         (void)fprintf(stderr, "[%5u]\t%10s\t%10s\n", i, nlinstr2str(instrs[i]), nlinstr2str(instrs2[i]));
      }
   }

   if (adiff) {
      (void)fprintf(stderr, "ERROR: differences in arguments for test %s\n", testname);
      for (u32 i = 0; i < len; ++i) {
         (void)fprintf(stderr, "[%5u]\t%5d\t%5d\n", i, args[i], args2[i]);
      }
   }

   if (idiff || adiff) {
      (void)fprintf(stderr, "%s p and i:\n", testname);
      for (u32 i = 0; i < len+1; ++i) { (void)fprintf(stderr, "[%5u] %u\n", i, otree->p[i]); }
      for (u32 i = 0, l = otree->idx_sz; i < l; ++i) { (void)fprintf(stderr, "[%5u] %u\n", i, otree->i[i]); }
      rc = 1;
   }


exit_:
   myfreeenvval(do_display);

   return rc;
}

int main(void) {

  /* ----------------------------------------------------------------------
   * We test the following:
   * 1) linearity detection in equation
   * 2) degree computations
   * 3) construction of the tree associated with the opcode
   * 
   * TODO:
   * a) conversion from nlopcode to reshop tree
   * b) linear conversion / extraction
   * c) quadratic conversion / extraction
   * ---------------------------------------------------------------------- */

   u32 err = 0;
#define DEF(x) if (OPARGS(x)[0] != ARRAY_SIZE(OPARGS(x))) { \
   (void)fprintf(stderr, "ERROR: inconstent sizes for " #x ": expected %zu, got %u\n", ARRAY_SIZE(OPARGS(x)), OPARGS(x)[0]); err += 1; }
TESTS_NLOPCODES()
#undef DEF

   if (err) { goto _exit; }

   err = 0;
#define DEF(x) if (test_compute_degree(OPCODES(x), OPARGS(x), DEGREE(x), #x)) { err += 1; }
TESTS_NLOPCODES()
#undef DEF

   if (err) { goto _exit; }

   /* */
#define DEF(x) if (test_nlopcode2dot(OPCODES(x), OPARGS(x), #x)) { err +=1; }
TESTS_NLOPCODES()
#undef DEF

   if (err) { goto _exit; }

   /* */
#define DEF(x) if (test_nlopcode2otree(OPCODES(x), OPARGS(x), #x)) { err +=1; }
TESTS_NLOPCODES()
#undef DEF

   if (err) { goto _exit; }

   return EXIT_SUCCESS;

_exit:
   if (err > 0) {
      (void)fprintf(stderr, "\n%u FATAL ERRORS!\n", err);
      return EXIT_FAILURE;
   } else {
      (void)fputs("\nSUCCESS!\n", stderr);
   }
}
