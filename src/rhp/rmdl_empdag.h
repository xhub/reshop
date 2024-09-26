#ifndef RMDL_EMPDAG_H
#define RMDL_EMPDAG_H


#include "empdag_data.h"
#include "reshop_data.h"

#include "rhp_fwd.h"

typedef struct {
   MpIdArray VFdag_starts;
   MpIdArray VFdag_mpids;
   bool need_sorting;
   unsigned max_size_subdag;
   unsigned num_newequs;
   unsigned *mpbig_vars;
   unsigned *mpbig_cons;
} VFContractions;

void vf_contractions_init(VFContractions *contractions) NONNULL;
void vf_contractions_empty(VFContractions *contractions) NONNULL;

int rmdl_contract_subtrees(Model *mdl_dst, VFContractions *contractions) NONNULL;
int rmdl_contract_along_Vpaths(Model *mdl_src, Model **mdl_dst) NONNULL;


#endif
