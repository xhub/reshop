#ifndef OPCODE_DIFF_OPS_H
#define OPCODE_DIFF_OPS_H

extern const struct sd_ops opcode_diff_ops;
int opcode_diff(int **deriv_instrs, int **deriv_args, int *pderiv_maxlen, const int *instrs, const int *args, int vidx, char *errmsg);

#endif /* OPCODE_DIFF_OPS_H  */
