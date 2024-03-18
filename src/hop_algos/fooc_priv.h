#ifndef FOOC_PRIV_H
#define FOOC_PRIV_H

#include "fooc_data.h"
#include "rhp_fwd.h"
#include "reshop_data.h"

typedef struct rosettas {
   UIntArray mdl_n;
   UIntArray rosetta_starts;
   ObjArray mdls;
   rhp_idx *data;
} Rosettas;

void rosettas_init(Rosettas *r) NONNULL;
void rosettas_free(Rosettas *r);


int compute_all_rosettas(Model *mdl_mcp, struct rosettas *r) NONNULL;
int fooc_check_childless_mps(Model *mdl_mcp, FoocData *fooc_dat) NONNULL;
bool childless_mp(const EmpDag *empdag, mpid_t mpid) NONNULL;

#endif /* FOOC_PRIV_H */
