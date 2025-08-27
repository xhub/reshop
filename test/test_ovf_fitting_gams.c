#include <stdio.h>

#include "reshop.h"

#include "test_ovf_fitting_common.h"
#include "test_gams_utils.h"

#define RESHOP_CHECK(EXPR) { int rc = EXPR; if (rc != RHP_OK) { status = rc; goto _exit; } }

/*  For now we don't test multiple GAMS solvers */
size_t nb_equil_solvers = 1;
size_t nb_nlp_solvers = 1;

int main (void)
{
  int status = 0;

   setup_gams();

  struct rhp_mdl *mdl_solver = NULL;

  for (size_t i = 0; i < loss_fns_size; ++i) {
    for (size_t j = 0; j < reformulations_size; ++j) {
      mdl_solver = rhp_mdl_new(RhpBackendGamsGmo);
      if (!mdl_solver) { status = 1; goto _exit; }

      // TODO: test GAMS with multiple solvers
      //ctr_setsolvername(ctr_solver, qp_solvers[l]);
      RESHOP_CHECK(fitting_test(mdl_solver, loss_fns[i], reformulations[j]));

      rhp_mdl_free(mdl_solver);
    }
  }

  for (size_t i = 0; i < loss_fns_size; ++i) {
    for (size_t j = 0; j < reformulations_size; ++j) {
      for (unsigned k = 1; k <= 5 ; ++k) {
        for (size_t l = 0; l < nb_nlp_solvers; ++l) {
          mdl_solver = rhp_mdl_new(RhpBackendGamsGmo);
          if (!mdl_solver) { status = 1; goto _exit; }

          //ctr_setsolvername(ctr_solver, qp_solvers[l]);

          RESHOP_CHECK(fitting_v_test(mdl_solver, loss_fns[i], reformulations[j], k));

          rhp_mdl_free(mdl_solver);
          mdl_solver = NULL;
        }
      }
    }
  }


  for (size_t i = 0; i < loss_fns_size; ++i) {
    for (size_t j = 0; j < reformulations_size; ++j) {
       for (size_t l = 0; l < nb_equil_solvers; ++l) {
         mdl_solver = rhp_mdl_new(RhpBackendGamsGmo);
         if (!mdl_solver) { status = 1; goto _exit; }

         RESHOP_CHECK(fitting_equil_test(mdl_solver, loss_fns[i], reformulations[j]));

         rhp_mdl_free(mdl_solver);
         mdl_solver = NULL;
       }
    }
  }

   mdl_solver = rhp_mdl_new(RhpBackendGamsGmo);
  if (!mdl_solver) { status = 1; goto _exit; }

  RESHOP_CHECK(fitting_test(mdl_solver, "elastic_net", "fenchel"));

  rhp_mdl_free(mdl_solver);
  mdl_solver = NULL;

_exit:
  rhp_mdl_free(mdl_solver);

  return status == RHP_OK ? 0 : 1;
}
