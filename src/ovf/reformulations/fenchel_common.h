#ifndef FENCHEL_COMMON_H
#define FENCHEL_COMMON_H

#include <stdbool.h>

#include "cones.h"
#include "mdl_data.h"
#include "ovfinfo.h"
#include "rhp_LA.h"
#include "rhp_fwd.h"

typedef struct {
   double * restrict tilde_y;      /**< change in y to make it belong to a cone*/
   double * restrict var_ub;
   Cone   * restrict cones_y;
   void  ** restrict cones_y_data;
   bool has_shift;       /**< True if we shift y to make it fit in a cone     */
   unsigned n_y_ub;
   unsigned n_y;
   double quad_cst;      /**< Quadratic constant -1/2 <tilde_y, M tilde_y>    */
} yData;

typedef struct  {
   unsigned n_v;
   rhp_idx start_vi_v_ub;
   rhp_idx *revidx_v_ub;
   Avar v;
} vData;

typedef struct {
   SpMat At;
   SpMat D;
   SpMat J;
   SpMat M;
   SpMat B_lin;
   double *a;              /**< The cost vector for the dual                  */
   double *b_lin;
   double *tmpvec;         /**< Temporary vector for matrix-vector products   */
   bool has_set;
   bool is_quad;
   RhpSense sense;
   OvfType type;
   unsigned nvars;
   unsigned nargs;
   rhp_idx vi_start;
   rhp_idx ei_start;
   rhp_idx idx;           /**< Index number, either variable or MP index      */
   yData ydat;            /**< Primal variable y */
   vData vdat;            /**< Dual variable v, multiplier on constraints     */
   Avar s_var;            /**< Dual variable s, for quadratic part            */
   bool *equ_gen;         /**< marker for generated equations                 */
   const OvfOps *ops;
   MathPrgm *mp;          /**< MathPrgm linked to the Ccf                     */
   char *equvar_basename; /**< Basename for new equations and variables       */
   OvfOpsData ovfd;
} CcfFenchelData;


NONNULL int fdat_init(CcfFenchelData * restrict fdat, OvfType type,
                      const OvfOps *ops, OvfOpsData ovfd, Model *mdl);
void fdat_empty(CcfFenchelData * restrict fdat);
NONNULL int fenchel_apply_yshift(CcfFenchelData *fdat);
NONNULL int fenchel_find_yshift(CcfFenchelData *fdat);

NONNULL int fenchel_gen_vars(CcfFenchelData *fdat, Model *mdl);
NONNULL int fenchel_gen_equs(CcfFenchelData *fdat, Model *mdl);

#endif
