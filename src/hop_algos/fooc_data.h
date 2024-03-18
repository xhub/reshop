#ifndef FOOC_DATA_H
#define FOOC_DATA_H

#include "compat.h"
#include "empdag_data.h"
#include "equ.h"
#include "mdl_data.h"
#include "reshop_data.h"

typedef struct {
   McpInfo *info;
   rhp_idx *vi_primal2ei_F;
   rhp_idx *ei_F2vi_primal;
   rhp_idx *mp2objequ;
   rhp_idx ei_F_start;
   rhp_idx ei_cons_start;
   rhp_idx ei_lincons_start;
   rhp_idx vi_mult_start;
   unsigned n_equs_orig;
   struct {
      unsigned total_m;
      unsigned total_n;
   } src;
   unsigned skipped_equ;
   Aequ objequs;
   Aequ cons_nl;
   Aequ cons_lin;
   MpIdArray mps;
} FoocData;

void fooc_data_init(FoocData *fooc_dat, McpInfo *info) NONNULL;
void fooc_data_fini(FoocData *fooc_dat);
int fooc_data_empdag(FoocData *fooc_dat, EmpDag *empdag, daguid_t uid_root, Fops *fops)
NONNULL_AT(1,2);

#endif
