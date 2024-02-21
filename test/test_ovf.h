#ifndef RHP_TEST_BASIC
#define RHP_TEST_BASIC

struct rhp_mdl;

int linear_quantile_regression(struct rhp_mdl *mdl, struct rhp_mdl *mdl_solver,
                               const char *formulation, unsigned s, const double xsols[3]);

#ifdef HAS_VREPR
#define ALL_LP_MODELS() \
    SOLVE(test_linear_quantile_regression_fenchel); \
    SOLVE(test_s_linear_quantile_regression_fenchel); \
    SOLVE(test_s_linear_quantile_regression_conjugate);

#else

#define ALL_LP_MODELS() \
    SOLVE(test_linear_quantile_regression_fenchel); \
    SOLVE(test_s_linear_quantile_regression_fenchel);

#endif

#define ALL_QP_MODELS()

#define ALL_EMP_MODELS() \
    SOLVE(test_s_linear_quantile_regression_equilibrium); \
    SOLVE(test_linear_quantile_regression_equilibrium); \


#undef SOLVE
#define SOLVE(TEST) int TEST(struct rhp_mdl *mdl, struct rhp_mdl *mdl_solver)

ALL_LP_MODELS()
ALL_QP_MODELS()
ALL_EMP_MODELS()

#endif
