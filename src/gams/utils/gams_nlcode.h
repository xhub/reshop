#ifndef GAMS_NLCODE
#define GAMS_NLCODE

#include "compat.h"
#include "rhp_defines.h"
#include "rhp_fwd.h"

typedef struct {
   rhp_idx idx;
   int len;
   u32 idx_sz;
   u32 root;
   const int *instr;
   const int *args;
   u32 *p;
   u32 *i;
   u32 *node_stack;
} GamsOpCodeTree;

int gams_nlcode2dot(Model *mdl, const int * restrict instr,
                    const int * restrict args,
                    char **fname_dot) NONNULL_AT(4);
// FIXME: is this necessary?
//u32 compute_nlcode_tree_sizes(int len, int instr[VMT(static restrict len)],
//                 int args[VMT(static restrict len)]);

void gams_opcodetree_free(GamsOpCodeTree* otree);
GamsOpCodeTree* gams_opcodetree_new(int len, const int instr[VMT(static restrict len)],
                                    const int args[VMT(static restrict len)]);
int gams_otree2instrs(Model *mdl, GamsOpCodeTree *otree, int **instrs, int **args) NONNULL_AT(2, 3, 4);

int gams_opcodetree2dot(Model *mdl, GamsOpCodeTree* otree, char **fname_dot);

#endif

