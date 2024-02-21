#ifndef FOOC_PRIV_H
#define FOOC_PRIV_H

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

#endif /* FOOC_PRIV_H */
