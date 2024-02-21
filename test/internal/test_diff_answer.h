#ifndef TEST_DIFF_ANSWER_H
#define TEST_DIFF_ANSWER_H

/* Derivative of -(x2*x2) w.r.t. x2: -(x2+x2) */
int ex1_diff_opcodes[] = {
   nlHeader,
   nlPushI,
   nlMulV,
   nlPushV,
   nlAdd,
   nlUMin,
   nlStore
};
int ex1_diff_fields[] = { 7, 1, 2, 2, 0, 0, 1 };

/* Derivative of -(x2*x2 + x2*x3): -(x2+x2+x3) */
int ex2_diff_opcodes[] = {
   nlHeader,
   nlPushI,
   nlMulV,
   nlPushV,
   nlAdd,
   nlPushI,
   nlMulV,
   nlAdd,
   nlUMin,
   nlStore
};
int ex2_diff_fields[] = { 10, 1, 2, 2, 0, 1, 3, 0, 0, 1 };

/* Derivative of -(x2*x2 + exp(x2)): -(x2+x2+exp(x2)) */
int ex3_diff_opcodes[] = {
   nlHeader,
   nlPushI,
   nlMulV,
   nlPushV,
   nlAdd,
   nlPushI,
   nlPushV,
   nlCallArg1,
   nlMul,
   nlAdd,
   nlUMin,
   nlStore
};
int ex3_diff_fields[] = { 12, 1, 2, 2, 0, 1, 2, 10, 0, 0, 0, 1 };

/* Derivative of -(3*x2**2): -((2*pow(x2,1))*3) */
int ex4_diff_opcodes[] = {
   nlHeader,
   nlPushI,
   nlPushI,
   nlMul,
   nlPushV,
   nlPushI,
   nlSubI,
   nlCallArg2,
   nlMul,
   nlMulI,
   nlUMin,
   nlStore
};
int ex4_diff_fields[] = { 12, 1, 6, 0, 2, 6, 1, 75, 0, 15, 0, 1 };

/* Derivative of -(3*x2**2 + exp(x2*x3)): -((2*pow(x2,1))*3 + exp(x2*x3)*x3) */
int ex5_diff_opcodes[] = {
   nlHeader,
   nlPushI,
   nlPushI,
   nlMul,
   nlPushV,
   nlPushI,
   nlSubI,
   nlCallArg2,
   nlMul,
   nlMulI,
   nlPushI,
   nlMulV,
   nlPushV,
   nlMulV,
   nlCallArg1,
   nlMul,
   nlAdd,
   nlUMin,
   nlStore
};
int ex5_diff_fields[] = { 19, 1, 6, 0, 2, 6, 1, 75, 0, 15, 1, 3, 2, 3, 10,
                          0, 0, 0, 1};

/* Derivative of -(x2/(1+x3)) w.r.t. x3: -(-x2/sqr(1+x3)) */
int ex6_diff_opcodes[] = {
   nlHeader,
   nlPushI,
   nlPushV,
   nlMul,
   nlUMin,
   nlPushI,
   nlAddV,
   nlCallArg1,
   nlDiv,
   nlUMin,
   nlStore
};
int ex6_diff_fields[] = { 11, 1, 2, 0, 0, 1, 3, 9, 0, 0, 1 };

/* Derivative of -(log(1+x2)) w.r.t. x2: -(1/(1+x2)) */
int ex7_diff_opcodes[] = {
   nlHeader,
   nlPushI,
   nlPushI,
   nlPushI,
   nlAddV,
   nlDiv,
   nlMul,
   nlUMin,
   nlStore
};
int ex7_diff_fields[] = { 9, 1, 1, 1, 2, 0, 0, 0, 1 };

#endif /* TEST_DIFF_ANSWER_H */
