#include <assert.h>

#include "ctr_rhp.h"
#include "equ.h"
#include "nltree.h"
#include "macros.h"
#include "mdl.h"
#include "mdl_rhp.h"
#include "rhp_fwd.h"

int nltree_get_add_node(Model* mdl, rhp_idx ei, unsigned n_children,
                        NlNode **add_node, unsigned *offset, double *coeff)
{
   Container *ctr = &mdl->ctr;
   assert(ei < ctr->m);

   Equ *e = &ctr->equs[ei];

   /* ----------------------------------------------------------------------
    * Get the tree and initialize it if needed
    * ---------------------------------------------------------------------- */
   S_CHECK(rctr_getnl(ctr, e));

   if (!e->tree) {
      S_CHECK(nltree_bootstrap(e, 2*n_children, n_children));
      *add_node = e->tree->root;
      *coeff = 1.;
      *offset = 0;

      return OK;
   }

   NlTree *tree = e->tree;
   S_CHECK(nltree_find_add_node(tree, &add_node, ctr->pool, coeff));
   
   return nltree_reserve_add_node(tree, add_node, n_children, offset);
}
