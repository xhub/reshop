#include <assert.h>

#include "cmat.h"
#include "consts.h"
#include "ctr_rhp.h"
#include "ctrdat_rhp.h"
#include "equ_modif.h"
#include "equvar_helpers.h"
#include "lequ.h"
#include "mathprgm.h"
#include "mdl.h"
#include "mdl_rhp.h"
#include "nltree.h"
#include "nltree_priv.h"
#include "rctr_equ_edit_priv.h"
#include "rmdl_priv.h"


/**
 * @brief Delete an equation from the model
 *
 * @param  mdl          the model
 * @param  ei           the equation to delete
 *
 * @return              the error code
 */
int rmdl_equ_rm(Model *mdl, rhp_idx ei)
{
   assert(mdl_is_rhp(mdl));

   Container *ctr = &mdl->ctr;
   RhpContainerData *ctrdat = (RhpContainerData *)ctr->data;

   S_CHECK(ei_inbounds(ei,  ctrdat->total_m, __func__));

   if (!ctrdat->equs[ei]) {
      error("[container] ERROR: equation %s is not active, cannot delete it\n",
            ctr_printequname(ctr, ei));
      return Error_NullPointer;
   }

   trace_ctr("[container] DEL equ %s\n", ctr_printequname(ctr, ei));

   S_CHECK(cmat_rm_equ(ctr, ei));

/*  TODO(xhub) SP revive that */
//   else if (new_indices) {
//      rosette->res.list = new_indices;
//   }
   //
   /* \TODO(xhub) check that we can forget an equation */
   ctr->m--;
   if (ctr->equmeta) {
      EquMeta *emeta = &ctr->equmeta[ei];

      unsigned mpid = emeta->mp_id;

      if (mpid_regularmp(mpid)) {
         MathPrgm *mp = empdag_getmpfast(&mdl->empinfo.empdag, mpid);
         IdxArray *mpequs = &mp->equs;
         int rc = rhp_idx_rmsorted(mpequs, ei);
         if (rc) {
            error("Failed to remove equation %s from MP(%s)",
                  ctr_printequname(ctr, ei),
                  empdag_getmpname(&mdl->empinfo.empdag, mpid));
            return Error_RuntimeError;
         }

      }
      emeta->ppty |= EquPptyIsDeleted;
   }

   return OK;
}

/** @brief Create an equation based on an existing one for editing
 *
 *  @param         mdl       the container
 *  @param[in,out] ei         on input, the equation to copy. On output, the index
 *                           off the new equation
 *
 *  @return the status of the operation
 */
int rmdl_dup_equ(Model *mdl, rhp_idx *ei)
{
   Container *ctr = &mdl->ctr;
   RhpContainerData *cdat = (RhpContainerData *)ctr->data;

   rhp_idx ei_src = *ei;
   S_CHECK(ei_inbounds(ei_src, cdat->total_m, __func__));

   /* \TODO(xhub) NAMING */

   char name[SHORT_STRING];
   S_CHECK(ctr_copyequname(ctr, ei_src, name, sizeof(name)/sizeof(char)));
   size_t indx = strlen(name);
   char name2[SHORT_STRING];
   snprintf(name2, SHORT_STRING, "_s%u", cdat->current_stage);
   size_t indx2 = strlen(name2);
   char *name_;
   MALLOC_(name_, char, 1 + indx + indx2);
   strcpy(name_, name);
   strcat(name_, name2);

   S_CHECK(cdat_equname_start(cdat, name_));
   rhp_idx ei_new = IdxNA;
   S_CHECK(rctr_add_equ_empty(ctr, &ei_new, NULL, ctr->equs[ei_src].object, ctr->equs[ei_src].cone));
   cdat_equname_end(cdat);

   S_CHECK(equ_dup(ctr, ei_src, ei_new));

   S_CHECK(cmat_copy_equ(ctr, ei_src, ei_new));

   trace_ctr("[container] DUP equ '%s' into '%s'\n",
             ctr_printequname(ctr, ei_src), ctr_printequname2(ctr, ei_new));

   S_CHECK(rmdl_equ_rm(mdl, ei_src));

   /* Update the rosetta info  */
   cdat->equ_rosetta[ei_src].res.equ = ei_new;

   /* Update the model info  */
   rhp_idx objequ;
   rmdl_getobjequ_nochk(mdl, &objequ);
   if (objequ == ei_src) {
      rmdl_setobjfun(mdl, ei_new);
   }

   *ei = ei_new;

   return OK;
}

/** @brief Create an equation based on an existing one for editing
 *
 *  @param         mdl       the container
 *  @param[in,out] ei         on input, the equation to copy. On output, the index
 *                           off the new equation
 *  @param         lin_space additional space in the linear vector
 *  @param         vi_no     if valid, the variable index not to copy
 *
 *  @return the status of the operation
 */
int rmdl_equ_dup_except(Model *mdl, rhp_idx *ei, unsigned lin_space, rhp_idx vi_no)
{
   Container *ctr = &mdl->ctr;
   RhpContainerData *cdat = (RhpContainerData *)ctr->data;

   rhp_idx ei_src = *ei;
   S_CHECK(ei_inbounds(ei_src, cdat->total_m, __func__));

   /* \TODO(xhub) NAMING */

   char name[SHORT_STRING];
   S_CHECK(ctr_copyequname(ctr, ei_src, name, sizeof(name)/sizeof(char)));
   size_t indx = strlen(name);
   char name2[SHORT_STRING];
   snprintf(name2, SHORT_STRING, "_s%u", cdat->current_stage);
   size_t indx2 = strlen(name2);
   char *name_;
   MALLOC_(name_, char, 1 + indx + indx2);
   strcpy(name_, name);
   strcat(name_, name2);

   S_CHECK(cdat_equname_start(cdat, name_));
   rhp_idx ei_new = IdxNA;

   S_CHECK(rctr_reserve_equs(ctr, 1));

   S_CHECK(rctr_add_equ_empty(ctr, &ei_new, NULL, ctr->equs[ei_src].object, ctr->equs[ei_src].cone));
   cdat_equname_end(cdat);

   S_CHECK(equ_copy_to(ctr, ei_src, &ctr->equs[ei_new], ei_new, lin_space, vi_no));

   S_CHECK(cmat_copy_equ_except(ctr, ei_src, ei_new, vi_no));

   trace_ctr("[container] DUP equ '%s' into '%s'\n",
             ctr_printequname(ctr, ei_src), ctr_printequname2(ctr, ei_new));

   S_CHECK(rmdl_equ_rm(mdl, ei_src));

   /* Update the rosetta info  */
   cdat->equ_rosetta[ei_src].res.equ = ei_new;

   *ei = ei_new;

   /* Update the model info */
   rhp_idx objequ;
   rmdl_getobjequ_nochk(mdl, &objequ);
   if (objequ == ei_src) {

      rmdl_setobjfun(mdl, ei_new);

   } else if (ctr->equmeta) {

      /* -------------------------------------------------------------------
       * Keep the EMP tree consistent: add the new equation to the MP
       * ------------------------------------------------------------------- */

      ctr->equmeta[ei_new] = ctr->equmeta[ei_src];

      /* metadata has already been copied in equ_copy_to */
      EquMeta *emeta = &ctr->equmeta[ei_new];
      /* We don't want the equation to be marked as deleted */
      emeta->ppty &= ~EquPptyIsDeleted;

      if (equmeta_is_shared(emeta)) {
         TO_IMPLEMENT("Shared Equation");
      }

      mpid_t mpid = emeta->mp_id;
      if (!mpid_regularmp(mpid)) { return OK; }

      MathPrgm *mp;
      S_CHECK(empdag_getmpbyid(&mdl->empinfo.empdag, mpid, &mp))
      
      EquRole equrole = emeta->role;

      //HACK use of internal MP DS
      emeta->mp_id = MpId_NA;

      switch (equrole) {
      case EquObjective:
         assert(mp->type == MpTypeOpt);
         S_CHECK(mp_setobjequ(mp, ei_new));
         break;
      case EquViFunction: {
         /* HACK: try to use mp_addvipir*/
         rhp_idx vi_dual = emeta->dual;
         if (emeta->ppty & EquPptyHasDualVar) {
            assert(mdl_valid_vi_(mdl, vi_dual, __func__));
            VarMeta *vmeta = &ctr->varmeta[vi_dual]; assert(vmeta->dual == ei_src);
            vmeta->dual = ei_new;
         }
         rhp_idx_addsorted(&mp->equs, ei_new);
         emeta->mp_id = mpid;
         }
         break;
      case EquConstraint:
         S_CHECK(mp_addconstraint(mp, ei_new));
         break;

      default:
         error("[model] ERROR: duplicated equation has role %s. This is not supported yet\n",
               equrole2str(equrole));
         return Error_NotImplemented;
      }
   }

   return OK;
}

int rmdl_equ_defmap2map(Model *mdl, Equ **e, rhp_idx defvar)
{
  /* ----------------------------------------------------------------------
   * We want to find defvar in the linear part of the equation, then multiply
   * the coeff
   * ---------------------------------------------------------------------- */

   Container *ctr = &mdl->ctr;
   Equ *e_map = *e; assert(e_map);
   assert(e_map->object == DefinedMapping); assert(mdl_is_rhp(mdl));

   unsigned pos_dummy;
   double vi_coeff;
   Lequ *le = e_map->lequ;
   S_CHECK(lequ_find(le, defvar, &vi_coeff, &pos_dummy));

   if (pos_dummy == UINT_MAX) {
      error("[container] ERROR: could not find variable '%s' in equation '%s'",
            ctr_printvarname(ctr, defvar), ctr_printequname(ctr, e_map->idx));
      return Error_RuntimeError;
   }

   double coeff = -1/vi_coeff;

   rhp_idx ei = e_map->idx;

   /* The value of defvar is defined by the equation */
   S_CHECK(rctr_add_eval_equvar(&mdl->ctr, ei, defvar));

   if (rctr_equ_is_readonly(ctr, ei)) {
      S_CHECK(rmdl_equ_dup_except(mdl, &ei, 0, defvar));
      e_map = &ctr->equs[ei];
      *e = e_map;
   } else {
      S_CHECK(equ_rm_var(ctr, e_map, defvar));
      TO_IMPLEMENT("Deletion without copy");
      // HACK: update the model representation to reflect the deletion of the variable
   }

   e_map->object = Mapping;

   if (ctr->varmeta) {
      VarMeta *vmeta = &ctr->varmeta[defvar];

      mpid_t mpid = vmeta->mp_id;
      if (mpid_regularmp(mpid)) {
         MathPrgm *mp;
         S_CHECK(empdag_getmpbyid(&mdl->empinfo.empdag, mpid, &mp));

         S_CHECK(rhp_idx_rmsorted(&mp->vars, defvar));

         // HACK: USE of internal MP data structures
         if (mp->type == MpTypeOpt && mp->opt.objvar == defvar) {
            mp->opt.objvar = IdxNA;
         }

      }

      }

   return rctr_equ_scal(ctr, e_map, coeff);
}

/**
 * @brief Flip an equation: copy and remove the original one
 *
 * @param       mdl      the container
 * @param       ei       the equation to flip
 * @param[out]  ei_new   the flipped equation
 *
 * @return       the error code
 */
int rmdl_equ_flip(Model *mdl, rhp_idx ei, rhp_idx *ei_new)
{  
   Container *ctr = &mdl->ctr;
   RhpContainerData *cdat = ctr->data;

   /* \TODO(xhub) this is not optimal */
   char name[SHORT_STRING], *name_;
   S_CHECK(ctr_copyequname(ctr, ei, name, sizeof(name)/sizeof(char)));
   size_t namelen = strlen(name);
   MALLOC_(name_, char, strlen("_flipped") + namelen + 1);
   strcpy(name_, name);
   strcat(name_, "_flipped");

   S_CHECK(cdat_equname_start(cdat, name_));
   S_CHECK(rctr_add_equ_empty(ctr, ei_new, NULL, ctr->equs[ei].object, ctr->equs[ei].cone));
   cdat_equname_end(cdat);

   rhp_idx ei_flip = *ei_new;
   S_CHECK(equ_copy_to(ctr, ei, &ctr->equs[ei_flip], ei_flip, 0, IdxNA));

   S_CHECK(cmat_cpy_equ_flipped(ctr, ei, ei_flip));

  /* ----------------------------------------------------------------------
   * Update values:
   * - marginal and value get their sign flipped
   * - basis remains unchanged
   * ---------------------------------------------------------------------- */

   ctr->equs[ei_flip].multiplier = -ctr->equs[ei].multiplier;
   ctr->equs[ei_flip].value = -ctr->equs[ei].value;

  /* ----------------------------------------------------------------------
   * Flip the algebraic content
   * ---------------------------------------------------------------------- */

   Equ *e = &ctr->equs[ei_flip];

   /* TODO: TEST */
   if (!cone_1D_polyhedral(e->cone)) {
      error("[process] ERROR: equation %s is marked to be flipped, but it is "
            "not a simple (in)equality, rather a inclusion in the cone %s.",
            ctr_printequname(ctr, ei), cone_name(e->cone));
      return Error_UnsupportedOperation;
   }

   e->p.cst = -e->p.cst;

   if (e->cone == CONE_R_MINUS) {
      e->cone  = CONE_R_PLUS;
      e->basis = ctr->equs[ei].basis == BasisUpper ? BasisLower :
                                 ctr->equs[ei].basis;
   } else if (e->cone == CONE_R_PLUS)  {
      e->cone  = CONE_R_MINUS;
      e->basis = ctr->equs[ei].basis == BasisLower ? BasisUpper :
                                 ctr->equs[ei].basis;
   }

   Lequ *le = e->lequ;
   if (le) {

      double * restrict values = le->coeffs;
      for (unsigned i = 0, len = le->len; i < len; ++i) {
         values[i] = -values[i];
      }
   }

   S_CHECK(rctr_getnl(ctr, e));

   if (e->tree && e->tree->root) {
      NlNode *lnode, *root = e->tree->root;

      if (root->op == NlNode_Umin) {
         e->tree->root = root->children[0];
      } else {
         A_CHECK(lnode, nlnode_alloc_fixed_init(e->tree, 1));
         nlnode_default(lnode, NlNode_Umin);
         lnode->children[0] = root;
          e->tree->root = lnode;
      }
   }

   trace_ctr("[container] FLIPPED equ '%s' into '%s'\n",
             ctr_printequname(ctr, ei), ctr_printequname2(ctr, ei_flip));

   /* Update the rosetta info  */

   cdat->equ_rosetta[ei].res.equ = ei_flip;
   cdat->equ_rosetta[ei].ppty |= EQU_PPTY_FLIPPED;


  /* ----------------------------------------------------------------------
   * If needed, update equmetadata
   * ---------------------------------------------------------------------- */


   EquMeta *equmeta = ctr->equmeta;

   if (equmeta) {
      memcpy(&equmeta[ei_flip], &equmeta[ei], sizeof (EquMeta));

      mpid_t mpid = equmeta[ei].mp_id;

      if (valid_idx(mpid)) {
         /* HACK. TODO GITLAB #89*/
         equmeta[ei_flip].mp_id = MpId_NA;
         const EmpDag *empdag = &mdl->empinfo.empdag;
         MathPrgm *mp;

         S_CHECK(empdag_getmpbyid(empdag, mpid, &mp));
         /* We use mp_addequ as it just adds the equation index in mp->equs.
          * For instance, for VI, using mp_addconstraint would increase the
          * number of constraints ... */
         S_CHECK(mp_addequ(mp, ei_flip));
      }
   }

   S_CHECK(rmdl_equ_rm(mdl, ei));

   return OK;
}

