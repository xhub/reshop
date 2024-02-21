#ifndef TEST_INSTR_H
#define TEST_INSTR_H

/* -(x2*x2) */
int ex1_opcodes[] = {
   nlHeader,
   nlPushV,
   nlMulV,
   nlUMin,
   nlStore
};
int ex1_fields[] = { 5, 2, 2, 0, 1 };

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
int ex2_fields[] = { 8, 2, 2, 2, 3, 0, 0, 1 };

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
int ex3_fields[] = { 8, 2, 2, 2, 10, 0, 0, 1 };

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
int ex4_fields[] = { 7, 2, 6, 75, 15, 0, 1 };

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
int ex5_fields[] = { 11, 2, 6, 75, 15, 2, 3, 10, 0, 0, 1 };

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
int ex6_fields[] = { 7, 2, 1, 3, 0, 0, 1 };

/* -(log(1+x2)) */
int ex7_opcodes[] = {
   nlHeader,
   nlPushI,
   nlAddV,
   nlCallArg1,
   nlUMin,
   nlStore
};
int ex7_fields[] = { 6, 1, 2, 11, 0, 1 };


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

int ex8_fields[] = { 8, 1, 9, 6, 2, 9, 6, 1};

#endif /* TEST_INSTR_H */
