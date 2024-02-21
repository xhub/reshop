#ifndef RHP_TEST_BASIC
#define RHP_TEST_BASIC

struct rhp_mdl;

#define ALL_LP_MODELS()
#define ALL_QP_MODELS()

#define ALL_EMP_MODELS() \
    SOLVE(test_ecvarup_msp_ovf) \
    ;

//    SOLVE(test_ecvarup_msp_dag)



#undef SOLVE
#define SOLVE(TEST) int TEST(struct rhp_mdl *mdl, struct rhp_mdl *mdl_solver)

ALL_LP_MODELS()
ALL_QP_MODELS()
ALL_EMP_MODELS()

#endif
