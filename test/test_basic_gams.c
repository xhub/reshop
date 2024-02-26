#include <stdio.h>

#include "reshop.h"

#include "test_basic.h"
#include "test_common.h"
#include "test_gams_utils.h"

//const char* gams_lp_solvers[] = {"baron", "cbc", "conopt", "cplex", "knitro", "minos", "pathnlp", "mosek", "xpress"};
//// TODO: what breaks here?

#if defined(_WIN32)

const char* gams_lp_solvers[] = {"baron", "conopt", "cplex",  "pathnlp", "mosek"};
const char* gams_nlp_solvers[] = {"conopt", "knitro",  "pathnlp", "snopt", "mosek"};

#else

const char* gams_lp_solvers[] = {"baron", "conopt", "cplex", "minos", "pathnlp", "mosek"};
const char* gams_nlp_solvers[] = {"conopt", "knitro", "minos", "pathnlp", "snopt", "mosek"};

#endif


const char* gams_emp_solvers[] = {""};

/* This is to support solver-specific parameters.
 * However, its use, for enable/disable some checks is no longer relevant */
//#define RHP_DEFINE_STRUCT 1
//#define RHP_LOCAL_SCOPE 1
//#define RHP_LIST_PREFIX _slv_params
//#define RHP_LIST_TYPE slv_params
//#define RHP_ELT_TYPE struct solver_params *
//#include "namedlist_generic.inc"

//tlsvar struct slv_params slv_params;

int main(void)
{
  int status = 0;


   setup_gams();


#define SOLVE_TEST_INIT(TEST_FN, SOLVER_STR) \
    if (gams_skip_solver(SOLVER_STR)) { continue; } \
    struct rhp_mdl *mdl = test_init(); \
    struct rhp_mdl *mdl_gams = rhp_mdl_new(RHP_BACKEND_GAMS_GMO); \
    if (!mdl_gams) { status = 1; goto _exit; } \
 \
    rhp_mdl_setsolvername(mdl_gams, SOLVER_STR); \
 \
    printf("%s :: %s \n", #TEST_FN, SOLVER_STR); \
    int lstatus = TEST_FN(mdl, mdl_gams); \
    if (lstatus != RHP_OK) { \
       printf("%s :: %s failed with status %d!\n", #TEST_FN, SOLVER_STR, lstatus); \
       status++; \
    } \
    rhp_mdl_free(mdl_gams); \
    test_fini();

#define SOLVE_LP(TEST_FN) \
  for (size_t i = 0; i < sizeof(gams_lp_solvers)/sizeof(gams_lp_solvers[0]); ++i) { \
    SOLVE_TEST_INIT(TEST_FN, gams_lp_solvers[i]) \
  }

#define SOLVE_NLP(TEST_FN) \
  for (size_t i = 0; i < sizeof(gams_nlp_solvers)/sizeof(gams_nlp_solvers[0]); ++i) { \
    SOLVE_TEST_INIT(TEST_FN, gams_nlp_solvers[i]) \
  }

#define SOLVE_EMP(TEST_FN) \
  for (size_t i = 0; i < sizeof(gams_emp_solvers)/sizeof(gams_emp_solvers[0]); ++i) { \
    SOLVE_TEST_INIT(TEST_FN, gams_emp_solvers[i]) \
  }


#undef SOLVE
#define SOLVE(TEST_FN) SOLVE_LP(TEST_FN)
ALL_LP_MODELS();

#undef SOLVE
#define SOLVE(TEST_FN) SOLVE_NLP(TEST_FN)
ALL_QP_MODELS();

#undef SOLVE
#define SOLVE(TEST_FN) SOLVE_EMP(TEST_FN)
ALL_EMP_MODELS();

_exit:
  return status;
}
