#ifndef GAMS_NLUTILS_H
#define GAMS_NLUTILS_H

#include "rhp_fwd.h"

struct gams_opcodes_file {
   int *instrs;
   int *args;
   size_t * indx_start;
   size_t nb_equs;
   size_t len_opcode;
   size_t min_pool_len;
   size_t pool_len;
   double *pool;
};

struct gams_opcodes_file* gams_read_opcode(const char* filename, double **pool);
int gams_write_opcode(struct gams_opcodes_file* equs, const char* filename);
void gams_opcodes_file_dealloc(struct gams_opcodes_file* equs);

#endif
