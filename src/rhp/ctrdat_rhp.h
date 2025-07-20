#ifndef cdat_RHP_H
#define cdat_RHP_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#include "cdat_rhp_data.h"
#include "compat.h"
#include "equ.h"
/*  TODO(Xhub) rework the solvestatus */
#include "filter_ops.h"
#include "rhp_fwd.h"
#include "var.h"

/** @file cdat_rhp.h
 *
 *  @brief Internal ReSHOP model structure
 */

struct filter_subset;
struct var_genops;
struct ctr_mat_elt;
struct equvar_eval;
struct vnames_list;
struct tofree_list;

enum EQU_PPTY {
   EQU_PPTY_NONE            = 0,
   EQU_PPTY_EXPANDED        = 1,
   EQU_PPTY_COPY_IF_MODIF   = 2,
   EQU_PPTY_FLIPPED         = 4,
};

union equ_trans {
   rhp_idx equ;
   rhp_idx* list;
};

struct rosetta {
   uint8_t ppty;
   union equ_trans res;
};


typedef struct equ_info {
   rhp_idx ei;
   bool expanded;
   bool copy_if_modif;
   bool flipped;
} EquInfo;

struct auxmdl {
   unsigned len;
   unsigned max;
   FilterSubset **filter_subset;
};

/** Inherited equations  */
typedef struct e_inh {
   Aequ e;       /**< Inherited equations in the current indexspace   */
   Aequ e_src;   /**< Inherited equations in the original indexspace  */
} EquInherited;

/** Inherited variables  */
typedef struct v_inh {
   Avar v;       /**< Inherited variables in the current indexspace   */
   Avar v_src;   /**< Inherited variables in the original indexspace  */
} VarInherited;

typedef struct vnames {
   VecNamesType type;
   rhp_idx start;
   rhp_idx end;
   union {
      struct vnames_list *list;
      struct vnames_fooclookup *fooc_lookup;
   };
   struct vnames *next;
} VecNames;

struct scalar_names {
   unsigned max;
   const char **names;
};

struct sos_grp {
   Avar v;
   double *w;
};

struct sos_group {
   unsigned max;
   unsigned len;
   struct sos_grp *groups;
};

struct postproc_data {
   unsigned remove_objvars;
};

/**
 * @struct model_repr cdat_rhp.h
 *
 * In memory representation of a model
 */
typedef struct ctrdata_rhp {
   unsigned *m;                       /**< number of (active) equations            */
   unsigned *n;                       /**< number of (active) variables            */

   size_t total_m;               /**< total number of equations, including those
                                     deleted during model transformations     */
   size_t total_n;               /**< total number of variables, including those
                                     deleted during model transformations     */

   size_t max_m;                   /**< Maximum number of equations          */
   size_t max_n;                   /**< Maximum number of variables          */
   struct e_inh equ_inherited;     /**< Inherited equations                  */
   struct v_inh var_inherited;     /**< Inherited variables                  */
   struct e_inh equname_inherited; /**< Inherited equations names            */

   unsigned char current_stage;     /**< Index for the current model
                                         transformation                       */
   bool objequ_val_eq_objvar;       /**< Flag to trigger the addition */
   bool borrow_inherited;           /**< True if inherited data is borrowed */
   bool strict_alloc;               /**< If true, be strict about model size */

   struct ctr_mat_elt **equs;         /**< list of equations                    */
   struct ctr_mat_elt **vars;         /**< list of variables                    */
   struct ctr_mat_elt **last_equ;     /**< pointer to the last equation where a variable appears*/

   struct ctr_mat_elt **deleted_equs; /**< list of deleted equations            */

   struct rosetta *equ_rosetta;

   unsigned char *equ_stage;        /**< Correspondence between stage and model
                                         transformation                       */

   struct auxmdl *stage_auxmdl;

   union {
      struct vnames v;
      struct scalar_names s;
   } equ_names;

   union {
      struct vnames v;
      struct scalar_names s;
   } var_names;

   unsigned equvar_evals_size;
   struct postproc_data pp;
   struct equvar_eval *equvar_evals;
   struct tofree_list *mem2free;
   struct sos_group sos1;
   struct sos_group sos2;
} RhpContainerData;

/*  TODO(xhub) see if it is a good idea to implement that */
//   struct model_elt * last_var;
/* pointer to the last variable in an equation*/


int cdat_alloc(Container *ctr, unsigned max_n, unsigned max_m) NONNULL;
int cdat_dealloc(Container *ctr, RhpContainerData* cdat);
int cdat_add_subctr(RhpContainerData* cdat, struct filter_subset* fs) NONNULL;
OWNERSHIP_TAKES(2)
int cdat_equname_start(RhpContainerData* cdat, char *name) NONNULL;
int cdat_equname_end(RhpContainerData* cdat) NONNULL;
int cdat_equ_init(RhpContainerData *cdat, rhp_idx ei) NONNULL;
int cdat_resize(RhpContainerData *cdat, unsigned max_n, unsigned max_m ) NONNULL;
OWNERSHIP_TAKES(2)
int cdat_varname_start(RhpContainerData* cdat, char *name) NONNULL;
int cdat_varname_end(RhpContainerData* cdat ) NONNULL;
int cdat_add2free(RhpContainerData *cdat, void *mem) NONNULL;

static NONNULL inline
rhp_idx cdat_ei_upstream(const RhpContainerData *cdat, rhp_idx ei)
{
   const struct e_inh *e_inh = &cdat->equ_inherited;
   unsigned idx = aequ_findidx(&e_inh->e, ei); 

   if (valid_idx(idx)) {
      return aequ_fget(&e_inh->e_src, idx);
   }

   return IdxNotFound;
}

static NONNULL inline
rhp_idx cdat_equname_inherited(const RhpContainerData *cdat, rhp_idx ei)
{
   const struct e_inh *ename_inh = &cdat->equname_inherited;
   unsigned idx = aequ_findidx(&ename_inh->e, ei); 

   if (valid_idx(idx)) {
      return aequ_fget(&ename_inh->e_src, idx);
   }

   return IdxNotFound;
}
static NONNULL inline
bool cdat_equ_isinherited(const RhpContainerData *cdat, rhp_idx ei)
{
   const struct e_inh *e_inh = &cdat->equ_inherited;
   return aequ_contains(&e_inh->e, ei);
}

static NONNULL inline
rhp_idx cdat_vi_upstream(const RhpContainerData *cdat, rhp_idx vi)
{
   const struct v_inh *v_inh = &cdat->var_inherited;
   unsigned idx = avar_findidx(&v_inh->v, vi); 

   if (valid_idx(idx)) {
      return avar_fget(&v_inh->v_src, idx);
   }

   return IdxNotFound;
}

static NONNULL inline
bool cdat_var_isinherited(const RhpContainerData *cdat, rhp_idx vi)
{
   const struct v_inh *v_inh = &cdat->var_inherited;
   return avar_contains(&v_inh->v, vi);
}

#endif
