#include <stdio.h>

#include "reshop.h"

#include "test_ovf_fitting_common.h"

#define RESHOP_CHECK(EXPR) { status = EXPR; if (status != RHP_OK) { goto _exit; } }

#define OVF_LOSS_FN "huber"

const char *qp_solvers[] = {
  "path",
//  "pathvi",
};
const size_t nb_qp_solvers = sizeof qp_solvers/sizeof qp_solvers[0];

const char *nlp_solvers[] = {
  "path",
//  "pathvi",
};
const size_t nb_nlp_solvers = sizeof nlp_solvers/sizeof nlp_solvers[0];

const char *equil_solvers[] = {
  "path",
//  "pathvi",
};
const size_t nb_equil_solvers = sizeof equil_solvers/sizeof equil_solvers[0];

struct opt {
  const char *name;
  union rhp_optval v;
};

/* For testing the options */
struct opt opts[] = {
  { .name = "ovf_init_new_variables", .v.bval = true}
};

int main (void)
{
  int status = 0;
  struct rhp_mdl *mdl_solver = NULL;

  for (size_t i = 0; i < loss_fns_size; ++i) {
    for (size_t j = 0; j < reformulations_size; ++j) {
       for (size_t l = 0; l < nb_qp_solvers; ++l) {
         mdl_solver = rhp_mdl_new(RhpBackendReSHOP);
         if (!mdl_solver) { status = 1; goto _exit; }

         rhp_mdl_setsolvername(mdl_solver, qp_solvers[l]);
         RESHOP_CHECK(fitting_test(mdl_solver, loss_fns[i], reformulations[j]));

         rhp_mdl_free(mdl_solver);
         mdl_solver = NULL;
       }
    }
  }

  for (size_t i = 0; i < loss_fns_size; ++i) {
    for (size_t j = 0; j < reformulations_size; ++j) {
      for (unsigned k = 1; k <= 5 ; ++k) {
        for (size_t l = 0; l < nb_nlp_solvers; ++l) {
          mdl_solver = rhp_mdl_new(RhpBackendReSHOP);
          if (!mdl_solver) { status = 1; goto _exit; }

          rhp_mdl_setsolvername(mdl_solver, nlp_solvers[l]);
          RESHOP_CHECK(fitting_v_test(mdl_solver, loss_fns[i], reformulations[j], k));

          rhp_mdl_free(mdl_solver);
          mdl_solver = NULL;
        }
      }
    }
  }

  for (size_t i = 0; i < loss_fns_size; ++i) {
    for (size_t j = 0; j < reformulations_size-1; ++j) {
       for (size_t l = 0; l < nb_equil_solvers; ++l) {
         mdl_solver = rhp_mdl_new(RhpBackendReSHOP);
         if (!mdl_solver) { status = 1; goto _exit; }

         rhp_mdl_setsolvername(mdl_solver, equil_solvers[l]);
         RESHOP_CHECK(fitting_equil_test(mdl_solver, loss_fns[i], reformulations[j]));

         rhp_mdl_free(mdl_solver);
         mdl_solver = NULL;
       }
    }
  }

_exit:
  rhp_mdl_free(mdl_solver);

  return status == RHP_OK ? 0 : 1;
}
