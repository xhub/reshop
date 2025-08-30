#ifndef TEST_OVF_FITTING_COMMON_H
#define TEST_OVF_FITTING_COMMON_H

#include <stddef.h>

#include "compat.h"

static const char *loss_fns[] = {
  "elastic_net",
  "l1",
  "l2",
  "hinge",
  "huber",
  "hubnik",
  "soft_hinge",
  "vapnik",
};

static const char *reformulations[] = {
  "equilibrium",
  "fenchel",
};

#define loss_fns_size (sizeof(loss_fns)/sizeof(loss_fns[0]))
UNUSED static const size_t reformulations_size = sizeof(reformulations)/sizeof(reformulations[0]);

int fitting_test(struct rhp_mdl *mdl_solver, const char *loss_fn,
                 const char *reformulation);

int fitting_v_test(struct rhp_mdl *mdl_solver, const char *loss_fn,
                   const char *reformulation, unsigned ver);

int fitting_equil_test(struct rhp_mdl *mdl_solver, const char *loss_fn,
                       const char *reformulation);

#endif
