#ifndef TEST_INSTR_H
#define TEST_INSTR_H

#include "gams_nlcode_priv.h"
#include "instr.h"
#include "rhp_defines.h"

/* -(x2*x2) */
int ex1_opcodes[] = {
   nlHeader,
   nlPushV,
   nlMulV,
   nlUMin,
   nlStore
};
int ex1_args[] = { 5, 2, 2, 0, 1 };
u8 ex1_degree = 2;

/* -(x2*x2 + x2*x3) */
int ex2_opcodes[] = {
   nlHeader,
   nlPushV,
   nlMulV,
   nlPushV,
   nlMulV,
   nlAdd,
   nlUMin,
   nlStore
};
int ex2_args[] = { 8, 2, 2, 2, 3, 0, 0, 1 };
u8 ex2_degree = 2;

/* -(x2*x2 + exp(x2)) */
int ex3_opcodes[] = {
   nlHeader,
   nlPushV,
   nlMulV,
   nlPushV,
   nlCallArg1,
   nlAdd,
   nlUMin,
   nlStore
};
int ex3_args[] = { 8, 2, 2, 2, 10, 0, 0, 1 };
u8 ex3_degree = DegFullyNl;

/* -(3*x2**2) */
int ex4_opcodes[] = {
   nlHeader,
   nlPushV,
   nlPushI,
   nlCallArg2,
   nlMulI,
   nlUMin,
   nlStore
};
int ex4_args[] = { 7, 2, 6, 75, 15, 0, 1 };
u8 ex4_degree = 2;

/* -(3*x2**2 + exp(x2*x3)) */
int ex5_opcodes[] = {
   nlHeader,
   nlPushV,
   nlPushI,
   nlCallArg2,
   nlMulI,
   nlPushV,
   nlMulV,
   nlCallArg1,
   nlAdd,
   nlUMin,
   nlStore
};
int ex5_args[] = { 11, 2, 6, 75, 15, 2, 3, 10, 0, 0, 1 };
u8 ex5_degree = DegFullyNl;

/* -(x2/(1 + x3)) */
int ex6_opcodes[] = {
   nlHeader,
   nlPushV,
   nlPushI,
   nlAddV,
   nlDiv,
   nlUMin,
   nlStore
};
int ex6_args[] = { 7, 2, 1, 3, 0, 0, 1 };
u8 ex6_degree = deg_mkdiv(1, 1);

/* -(log(1+x2)) */
int ex7_opcodes[] = {
   nlHeader,
   nlPushI,
   nlAddV,
   nlCallArg1,
   nlUMin,
   nlStore
};
int ex7_args[] = { 6, 1, 2, 11, 0, 1 };
u8 ex7_degree = DegFullyNl;

/* p6*x1**2 + p6*x2**2 */
int ex8_opcodes[] = {
   nlHeader,
   nlPushV, 
   nlCallArg1, 
   nlMulI, 
   nlPushV, 
   nlCallArg1, 
   nlMulIAdd, 
   nlStore
};
int ex8_args[] = { 8, 1, 9, 6, 2, 9, 6, 1};
u8 ex8_degree = 2;

/* -(x2/(p1 + p3)) */
int lin1_opcodes[] = {
   nlHeader,
   nlPushV,
   nlPushI,
   nlAddI,
   nlDiv,
   nlUMin,
   nlStore
};
int lin1_args[] = { 7, 2, 1, 3, 0, 0, 1 };
u8 lin1_degree = 1;

/* -(p2*x2) */
int lin2a_opcodes[] = {
   nlHeader,
   nlPushI,
   nlMulV,
   nlUMin,
   nlStore
};
int lin2a_args[] = { 5, 2, 2, 0, 1 };
u8 lin2a_degree = 1;

/* -(x2*p8) */
int lin2b_opcodes[] = {
   nlHeader,
   nlPushV,
   nlMulI,
   nlUMin,
   nlStore
};
int lin2b_args[] = { 5, 2, 8, 0, 1 };
u8 lin2b_degree = 1;

/* -(x2*p20 + exp(p20)) */
int lin3_opcodes[] = {
   nlHeader,
   nlPushV,
   nlMulI,
   nlPushI,
   nlCallArg1,
   nlAdd,
   nlUMin,
   nlStore
};
int lin3_args[] = { 8, 2, 20, 20, 10, 0, 0, 1 };
u8 lin3_degree = 1;

#endif /* TEST_INSTR_H */
