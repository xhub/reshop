#include <stdio.h>

#include "colors.h"
#include "reshop.h"

#include "test_crm.h"
#include "test_common.h"

const char * reshop_lp_solvers[] = {"path"};

int main(void)
{
  int status = 0;

#undef SOLVE
#define SOLVE(TEST_FN) \
  for (size_t i = 0; i < sizeof(reshop_lp_solvers)/sizeof(reshop_lp_solvers[0]); ++i) { \
    struct rhp_mdl *mdl_reshop = rhp_mdl_new(RhpBackendReSHOP); \
    if (!mdl_reshop) { status = 1; goto _exit; } \
    struct rhp_mdl *mdl = test_init(); \
    /* ctr_setsolvername(ctr_reshop, reshop_lp_solvers[i]); */\
 \
    printf("%s :: %s \n", #TEST_FN, reshop_lp_solvers[i]); \
    int status_test = TEST_FN(mdl, mdl_reshop); \
    rhp_mdl_free(mdl_reshop); \
    test_fini(); \
    if (status_test) { \
      status = status_test; \
      printf(ANSI_COLOR_RED "%s :: solver %s failed with status %d!\n" ANSI_COLOR_RESET, #TEST_FN, reshop_lp_solvers[i], status); \
    } \
  }


ALL_LP_MODELS();
ALL_QP_MODELS()
ALL_EMP_MODELS()

_exit:
  return status;
}
