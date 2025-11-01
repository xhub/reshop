#ifndef FENCHEL_COMMON_H
#define FENCHEL_COMMON_H

#include <stdbool.h>

#include "cones.h"
#include "equ.h"
#include "mdl_data.h"
#include "ovfinfo.h"
#include "rhp_LA.h"
#include "rhp_fwd.h"

typedef struct {
   double * restrict tilde_y;      /**< ỹ: change in y to make it belong to a cone     */
   double * restrict var_ub;       /**< upper bounds of y, after shift        */
   Cone   * restrict cones_y;
   void  ** restrict cones_y_data;
   unsigned * restrict mult_ub_revidx;  /**< reverse index on the upper bound */
   bool has_yshift;       /**< True if we shift y by ỹ to make it fit in a cone     */
   unsigned n_y_ub;      /**< Number of upper bound contraints of y, after shift */
   unsigned n_y;         /**< Size of y                                       */
   double shift_quad_cst;      /**< Quadratic constant -1/2 <ỹ, M ỹ>    */
} yData;

typedef struct  {
   Avar v;                /**< multiplier on the constraints */
   Avar w;                /**< multiplier on the upper bound of y */
   Avar s;                /**< Dual variable s, for quadratic part            */
} DualVars;

typedef struct {
   double *a;              /**< The cost vector for the dual (v variable)     */
   double *ub;             /**< The cost vector for the dual (w variable)     */
   DualVars vars;
   Aequ cons;
   rhp_idx ei_objfn;      /**< Objective function */
} CcfFenchelDual;


typedef struct {
   bool has_set;
   bool is_quad;
   RhpSense sense;
   unsigned ncons;        /**< The number of constraints (conic inclusion)*/
   yData ydat;            /**< Primal variable y */
} CcfFenchelPrimal;

typedef struct {
   SpMat At;
   SpMat D;
   SpMat J;
   SpMat M;
   SpMat B_lin;
   double *b_lin;
   double *tmpvec;         /**< Temporary vector for matrix-vector products   */
   CcfFenchelPrimal primal;
   CcfFenchelDual dual;
   OvfType type;
   bool skipped_cons;     /**< True if some dual constraints were not created */
   bool finalize_equ;     /**< If true, finalize equations as they are generated */
   unsigned nargs;
   rhp_idx vi_ovf;        /**< OVF variable index                                      */
   bool *cons_gen;         /**< marker for generated equations                         */
   const OvfOps *ops;
   MathPrgm *mp_dst;      /**< MathPrgm that owns the new variables and equations      */
   mpid_t mpid_primal;    /**< Primal MP id                                            */
   mpid_t mpid_dual;      /**< Dual MP id                                              */
   char *equvar_basename; /**< Basename for new equations and variables                */
   OvfOpsData ovfd;
} CcfFenchelData;


NONNULL int fdat_init(CcfFenchelData * restrict fdat, OvfType type,
                      const OvfOps *ops, OvfOpsData ovfd, Model *mdl);
void fdat_empty(CcfFenchelData * restrict fdat);
NONNULL int fenchel_apply_yshift(CcfFenchelData *fdat);
NONNULL int fenchel_find_yshift(CcfFenchelData *fdat);

NONNULL int fenchel_gen_vars(CcfFenchelData *fdat, Model *mdl);
NONNULL int fenchel_gen_cons(CcfFenchelData *fdat, Model *mdl);
NONNULL int fenchel_gen_objfn(CcfFenchelData *fdat, Model *mdl);
int fenchel_edit_empdag(CcfFenchelData *fdat, Model *mdl);

#endif
