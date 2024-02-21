#ifndef RHP_TEST_BASIC
#define RHP_TEST_BASIC

struct rhp_mdl;

#define ALL_LP_MODELS() \
    SOLVE(test_oneobjvar1); \
    SOLVE(test_oneobjvar2); \
    SOLVE(test_oneobjvarcons1); \
    SOLVE(test_onevar); \
    SOLVE(test_onevarmax); \
    SOLVE(test_onevarmax2); \
    SOLVE(test_twovars1) \

//    SOLVE(test_feas)

#define ALL_QP_MODELS() \
    SOLVE(test_qp1); \
    SOLVE(test_qp2); \
    SOLVE(test_qp3); \
    SOLVE(test_qp4); \
    SOLVE(test_qp5); \
    SOLVE(test_qp1bis); \
    SOLVE(test_qp2bis); \
    SOLVE(test_qp3bis); \
    SOLVE(test_qp4bis); \
    SOLVE(test_qp5bis)  \


#define ALL_EMP_MODELS() \
    SOLVE(gnep_tragedy_common); \
    SOLVE(mopec)


#undef SOLVE
#define SOLVE(TEST) int TEST(struct rhp_mdl *mdl, struct rhp_mdl *mdl_solver)

ALL_LP_MODELS();
ALL_QP_MODELS();
ALL_EMP_MODELS();

#endif
