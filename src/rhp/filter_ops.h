#ifndef FILTER_OPS_H
#define FILTER_OPS_H

#include <stdbool.h>

#include "compat.h"
#include "empdag_data.h"
#include "mdl_data.h"
#include "rhp_fwd.h"

/**
 * @file filter_ops.h
 *
 * @brief utilities to filter variables and equations
 */

struct mp_descr;
typedef struct filter_subset FopsSubsetData;

typedef struct rhp_ctr_filter_ops {
   FopsType type;
   void *data;
   void (*freedata)(void *data) NONNULL;
   unsigned (*deactivatedequslen)(void *data) NONNULL;
   void (*get_sizes)(void *data, size_t* nvars, size_t* nequs) NONNULL ACCESS_ATTR(write_only, 2) ACCESS_ATTR(write_only, 3);
   bool (*keep_var)(void *data, rhp_idx vi) NONNULL;
   bool (*keep_equ)(void *data, rhp_idx ei) NONNULL;
   rhp_idx (*vars_permutation)(void *data, rhp_idx vi) NONNULL;
   int (*transform_lequ)(void *data, Equ *e, Equ *edst ) NONNULL;
   int (*transform_gamsopcode)(void *data, rhp_idx ei, int *instrs, int *args,
                               unsigned len ) NONNULL;
   int (*transform_nltree)(void *data, Equ *e, Equ *edst) NONNULL;
} Fops;

const char *fopstype_name(FopsType type);
int fops_active_init(Fops *ops, Container *ctr) NONNULL;
int fops_deactivate_equ(void *data, rhp_idx ei) NONNULL;
int fops_deactivate_var(void *data, rhp_idx vi) NONNULL;
void fops_subset_init(Fops *fops, struct filter_subset *fs ) NONNULL;
void fops_subset_release(FopsSubsetData *fs);
FopsSubsetData* filter_subset_new(unsigned vlen, Avar vars[vlen], unsigned elen,
                                  Aequ equs[elen], struct mp_descr* mp_d)
                                  MALLOC_ATTR(fops_subset_release,1) NONNULL_AT(5);

int filter_subset_activate(FopsSubsetData *fs, Model *mdl, unsigned offset_pool ) NONNULL;

int fops_subdag_init(Fops *fops, Model *mdl, daguid_t uid) NONNULL;
int fops_setvarpermutation(Fops *fops, rhp_idx *vperm) NONNULL;

#endif /* FILTER_OPS_H */
