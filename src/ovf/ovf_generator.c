#include <assert.h>
#include <math.h>

#include "container.h"
#include "macros.h"
#include "ctr_rhp.h"
#include "printout.h"
#include "ovf_generator.h"
#include "status.h"

int ovf_gen_id_matrix(unsigned n, const void *env, SpMat *mat)
{
   rhpmat_reset(mat);
   mat->csr = ovf_speye_mat(n, n, 1.);
   if (!mat->csr) return Error_InsufficientMemory;
   rhpmat_set_eye(mat);
   rhpmat_set_csr(mat);
   rhpmat_set_sym(mat);

   return OK;
}


/*  \TODO(xhub) pass matrix ppty to set */
struct sp_matrix* ovf_speye_mat_x(unsigned n, unsigned m, double value,
                                  bool ppty[])
{
   assert(n > 0);
   assert(m > 0);

   // CSR alloc
   struct sp_matrix* mat;

   if (m == n) {
      if (!ppty[FORCE_ALLOC]) {
         AA_CHECK(mat, rhpmat_spalloc(n, m, 1, RHP_CS));
         mat->x[0] = value;
      } else {
         AA_CHECK(mat, rhpmat_spalloc(n, m, m, RHP_CS));

         if (ppty[FILL_DATA]) {
            for (unsigned i = 0; i < n; ++i) {
               mat->x[i] = value;
            }
         } else if (ppty[INIT_DATA]) {
            memset(mat->x, m, sizeof(double));
         }
      }
   } else {
      // unsupported case follows
      if (m%n != 0 && n%m != 0) {
         error("%s :: m and n are not multiples! m = %d n = %d\n",
                  __func__, m, n);
         return NULL;
      }

      /* \TODO(xhub) use memcpy for mat->x */
      if (m > n) {
         AA_CHECK(mat, rhpmat_spalloc(n, m, m, RHP_CS));

         for (unsigned i = 0; i < m; ++i) {
            mat->i[i] = i%n;
            mat->x[i] = value;
            mat->p[i] = i;
         }
         mat->p[m] = m;
      } else {
         AA_CHECK(mat, rhpmat_spalloc(n, m, n, RHP_CS));

         unsigned k = 0;
         for (unsigned i = 0; i < m; ++i) {
            mat->p[i] = k;
            for (unsigned j = 0; j < n; j+=m, ++k) {
               mat->i[k] = i + j;
               mat->x[k] = value;
            }
         }
         mat->p[m] = n;
      }
   }

   mat->n = n;
   mat->m = m;

   return mat;
}

int ovf_box(Container* ctr, unsigned nb_vars, const void* env,
            const struct var_genops* varfill)
{
   S_CHECK(rctr_reserve_vars(ctr, nb_vars));
   S_CHECK(rctr_add_box_vars_ops(ctr, nb_vars, env, varfill));

   return OK;
}

