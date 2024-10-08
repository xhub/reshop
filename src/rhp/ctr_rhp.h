#ifndef CTR_RHP_H
#define CTR_RHP_H

#include <stdbool.h>

#include "cones.h"
#include "container.h"
#include "ctrdat_rhp.h"
#include "equ_data.h"
#include "rhp_fwd.h"

struct var_genops;
typedef struct equ_info EquInfo;

/**
 *  @defgroup CMatUpdated  Functions that update the container matrix
 *
 * When performing changes to equations, it is important to ensure consistency
 * of the container matrix with the equation data. All functions in this group
 * update the container matrix. This is not the case of all functions that
 * change equations
 */


/**
 *  @defgroup EquCreation  Functions to create an equation from scratch
 *
 * TLDR: these functions do not update the container matrix
 * When creating equations, one has more control over their content.
 *
 * Hence, one can create all the equation data and then enter the equation
 * in the container matrix once their are done.
 *
 */


/**
 *  @defgroup EquSafeEditing  Functions to edit existing equations
 *
 * When performing changes to equations, it is important to ensure consistency
 * of the container matrix with the equation data. All functions in this group
 * update the container matrix. This is not the case of all functions that
 * change equations. When creating equation, it might be faster to use the
 * equivalent functions in the \ref EquUnsafeEditing group.
 */


/**
 *  @defgroup EquUnsafeEditing  Functions to edit existing equations
 *
 * These function do not update the linear part of the matrix container
 */

/* -------------------------------------------------------------------------
 * rmdl_ensure_pool is known to look for uninit memory
 * ------------------------------------------------------------------------- */


int rhp_chk_ctr(const Container *ctr, const char *fn);

int rctr_delete_var(Container *ctr, rhp_idx vi);
NONNULL ACCESS_ATTR(write_only, 3)
int rctr_evalfunc(Container *ctr, rhp_idx ei, double * restrict F);
NONNULL ACCESS_ATTR(read_only, 3) ACCESS_ATTR(write_only, 4)
int rctr_evalfuncat(Container *ctr, Equ *e, const double * restrict x,
                    double * restrict F);
int rctr_evalfuncs(Container *ctr) NONNULL;
void rctr_inherited_equs_are_not_borrowed(Container *ctr);

int rctr_getnl(const Container* ctr, Equ *e);
unsigned rctr_poolidx(Container *ctr, double val);
int rctr_setequvarperp(Container *ctr, rhp_idx ei, rhp_idx vi);
int rctr_walkequ(const Container *ctr, rhp_idx ei, void **iterator,
                 double *jacval, rhp_idx *vi, int *nlflag);
int rctr_get_var_sos1(Container *ctr, rhp_idx vi, unsigned **grps);
int rctr_get_var_sos2(Container *ctr, rhp_idx vi, unsigned **grps);

NONNULL_AT(1, 2) ACCESS_ATTR(write_only, 2) ACCESS_ATTR(read_write, 3)
int rctr_add_equ_empty(Container *ctr, rhp_idx *ei, Equ **e,
                       EquObjectType type, enum cone cone);

int rctr_init_equ_empty(Container *ctr, rhp_idx ei, EquObjectType type,
                        enum cone cone) NONNULL;



int rctr_reserve_eval_equvar(Container *ctr, unsigned size) NONNULL;
int rctr_add_eval_equvar(Container *ctr, rhp_idx ei, rhp_idx vi ) NONNULL;
int rctr_func2eval_add(Container *ctr, rhp_idx ei) NONNULL;

int rctr_var_setasdualmultiplier(Container *ctr, Equ *e, rhp_idx vi) NONNULL;
int rctr_add_multiplier_dual(Container *ctr, enum cone cone, void* cone_data,
                              rhp_idx *vi) NONNULL_AT(1, 4);
int rctr_add_multiplier_polar(Container *ctr, enum cone cone, void* cone_data,
                               rhp_idx *vi) NONNULL_AT(1, 4);
int rctr_add_box_vars_ops(Container *ctr, unsigned nb_vars, const void* env,
                          const struct var_genops* varfill) NONNULL;
int rctr_fix_equ(Container *ctr, rhp_idx ei) NONNULL;


/* ----------------------------------------------------------------------
 * Equation modifications
 * ---------------------------------------------------------------------- */

int rctr_equ_addlinvars(Container *ctr, Equ *e, Avar *v, const double *vals) NONNULL;
int rctr_equ_addlinvars_coeff(Container *ctr, Equ *e, Avar *v, const double* vals,
                              double coeff) NONNULL;



int rctr_equ_addnewlvars(Container *ctr, Equ *e, Avar *v, const double* vals) NONNULL;
int rctr_equ_addnewlin_coeff(Container *ctr, Equ *e, Avar *v, const double *vals,
                             double coeff) NONNULL;
int rctr_equ_addnewvar(Container *ctr, Equ *e, rhp_idx vi, double val) NONNULL;
int rctr_equ_addlvar(Container *ctr, Equ *e, rhp_idx vi, double val) NONNULL;


NONNULL int rctr_equ_add_map(Container *ctr, Equ *edst, rhp_idx ei, rhp_idx vi_map,
                             double coeff);
int rctr_equ_add_newmap(Container *ctr, Equ *edst, rhp_idx ei, rhp_idx vi_map,
                        double coeff) NONNULL;
int rctr_nltree_copy_map(Container *ctr, NlTree *tree, NlNode **node,
                         Equ *esrc, rhp_idx vi_no, double coeff);
int rctr_equ_add_nlexpr(Container *ctr, rhp_idx ei, NlNode *node, double cst) NONNULL;

int rctr_set_equascst(Container *ctr, rhp_idx ei) NONNULL;

int rctr_equ_add_equ_rosetta(Container *ctr, Equ *dst, Equ *src, const rhp_idx* rosetta)
NONNULL_AT(1,2,3);
int rctr_equ_min_equ_rosetta(Container *ctr, Equ *dst, Equ *src, const rhp_idx* rosetta)
NONNULL_AT(1,2,3);
int rctr_equ_add_equ_coeff(Container *ctr, Equ *dst, Equ *src, double coeff) NONNULL;
int rctr_equ_add_equ_x(Container *ctr, Equ * restrict dst, Equ * restrict src,
                       double coeff, const rhp_idx* restrict rosetta)
NONNULL_AT(1,2,3);
int rctr_equ_addmulv_equ_coeff(Container *ctr, Equ *dst, Equ *src, rhp_idx vi, double coeff) NONNULL;
int rctr_equ_submulv_equ(Container *ctr, Equ *dst, Equ *src, rhp_idx vi) NONNULL;
int rctr_equ_submulv_equ_coeff(Container *ctr, Equ *dst, Equ *src, rhp_idx vi, double coeff) NONNULL;



int rctr_copyvar(Container *ctr, const Var * restrict vsrc) NONNULL;
int rctr_get_equation(const Container* ctr, rhp_idx ei, EquInfo * restrict equinfo) NONNULL;
int rctr_walkallequ(const Container *ctr, rhp_idx ei, void **iterator, double *val,
                    rhp_idx *vi, bool *nlflag ) NONNULL;

int rctr_add_init_equs(Container *ctr, unsigned nb);

int rctr_reserve_equs(Container *ctr, unsigned n_equs) NONNULL;
int rctr_reserve_vars(Container *ctr, unsigned n_vars) NONNULL;
size_t rctr_totaln(const Container *ctr) NONNULL;
size_t rctr_totalm(const Container *ctr) NONNULL;

int rctr_deactivate_equ(Container *ctr, rhp_idx ei) NONNULL;
int rctr_deactivate_var(Container *ctr, rhp_idx vi) NONNULL;
int rctr_var_in_equ(const Container *ctr, rhp_idx vi, rhp_idx ei, bool *res) NONNULL;

/* ----------------------------------------------------------------------
 * Reformulation of container data
 * ---------------------------------------------------------------------- */

int rmdl_ctr_transform(Model *mdl) NONNULL;

static inline rhp_idx rctr_maxn(const Container *ctr) {
  assert(ctr_is_rhp(ctr));
  return ((RhpContainerData*)ctr->data)->max_n;
}

static inline rhp_idx rctr_maxm(const Container *ctr) {
  assert(ctr_is_rhp(ctr));
  return ((RhpContainerData*)ctr->data)->max_m;
}
#endif
