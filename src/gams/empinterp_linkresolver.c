#include "asprintf.h"

#include "empdag.h"
#include "empdag_common.h"
#include "empinfo.h"
#include "empinterp.h"
#include "empinterp_linkresolver.h"
#include "empinterp_priv.h"
#include "empinterp_symbol_resolver.h"
#include "mathprgm.h"
#include "mdl.h"
#include "printout.h"
#include "status.h"

#include "dctmcc.h"
#include "toplayer_utils.h"

typedef struct label_dat {
   uint8_t dim;
   uint16_t label_len;
   const char *label;
   int uels[GMS_MAX_INDEX_DIM];
} LabelDat;

typedef struct arc_dat {
   LabelDat labeldat;
   LinkType type;
   union {
      ArcVFBasicData basic_dat;
      ArcVFMultipleBasicData basics_dat;
      ArcVFEquData equ_dat;
      ArcVFLequData lequ_dat;
   };
} ArcDat;


static void labeldat_print(LabelDat *labeldat, dctHandle_t dct, unsigned mode)
{
   printout(mode, "%.*s", labeldat->label_len, labeldat->label);
   unsigned dim = labeldat->dim;

   if (dim == 0) return;

   printstr(mode, "(");
   int dummyoffset;
   dct_printuel(dct, labeldat->uels[0], mode, &dummyoffset);


   for (unsigned i = 1; i < dim; ++i) {
      printstr(mode, ", ");
      dct_printuel(dct, labeldat->uels[i], mode, &dummyoffset);
   }

   printstr(mode, ")");
}

static daguid_t dagregister_find(const DagRegister *dagreg, LabelDat *labeldat)
{
   const char * restrict labelname = labeldat->label;

   if (!labelname) {
      errormsg("[empdag] ERROR: label is NULL!\n");
      return Error_NullPointer;
   }

   uint16_t labelname_len = labeldat->label_len;
   uint8_t edge_dim = labeldat->dim;
   int *uels = labeldat->uels;

   for (unsigned i = 0, len = dagreg->len; i < len; ++i) {
      const DagRegisterEntry *entry = dagreg->list[i];

      if (entry->label_len != labelname_len ||
          entry->dim != edge_dim ||
          strncasecmp(labelname, entry->label, labelname_len) != 0) {
         goto _loop;
      }

      for (unsigned j = 0, dim = entry->dim; j < dim; ++j) {
         if (uels[j] != entry->uels[j]) goto _loop;
      }

      return entry->daguid_parent;
_loop:
      ;
   }

   return UINT_MAX;
}

static int add_dualize_operation(EmpDag *empdag, mpid_t mpid_primal, mpid_t mpid_dual)
{
   char *name_dual;

   MathPrgm *mp_primal, *mp_dual;
   S_CHECK(empdag_getmpbyid(empdag, mpid_primal, &mp_primal));
   mp_hidable_asdual(mp_primal);

   const char *name_primal = empdag_getmpname(empdag, mpid_primal);
   IO_CALL(asprintf(&name_dual, "%s_dual", name_primal));
   S_CHECK(empdag_setmpname(empdag, mpid_dual, name_dual));
   free(name_dual);

   S_CHECK(empdag_getmpbyid(empdag, mpid_dual, &mp_dual));

   mp_dual->dual.mpid_primal = mpid_primal;

   mp_dual->sense = mp_primal->sense == RhpMin ? RhpMax : RhpMin;

   DualOperatorData *dualdat = &mp_dual->dual.dualdat;

   bool nodal_dom;

   switch (dualdat->domain) {
   case NodeDomain:   nodal_dom = true;  break;
   case SubDagDomain: nodal_dom = false; break;
   default:
      error("[empdag] ERROR: unsupported dual domain value %d\n", dualdat->domain);
      return Error_RuntimeError;
   }

   switch (dualdat->scheme) {
   case FenchelScheme:
      if (nodal_dom) {
         S_CHECK(mpidarray_addsorted(&empdag->fenchel_dual_nodal, mpid_dual));
      } else {
         S_CHECK(mpidarray_addsorted(&empdag->fenchel_dual_subdag, mpid_dual));
      }
      break;
   case EpiScheme:
      if (nodal_dom) {
         S_CHECK(mpidarray_addsorted(&empdag->epi_dual_nodal, mpid_dual));
      } else {
         S_CHECK(mpidarray_addsorted(&empdag->epi_dual_subdag, mpid_dual));
      }
      break;
   default:
      error("[empdag] ERROR: unsupported dual scheme value %d\n", dualdat->scheme);
      return Error_RuntimeError;
   }

   trace_empinterp("[empinterp] Setting MP(%s) as dual to MP(%s)\n",
                   empdag_getmpname(empdag, mpid_dual),
                   empdag_getmpname(empdag, mpid_primal));

   return OK;
}

//static int getfooc(EmpDag *empdag, daguid_t daguid_dst, daguid_t *daguid_fooc)
//{
//   return OK;
//}

static int err_wrong_node_type(Interpreter *interp, daguid_t uid_parent,
                               ArcDat *arcdat, const char *errmsg)
{
   EmpDag *empdag = &interp->mdl->empinfo.empdag;
   error("[empinterp] ERROR: %s. This error happened while processing the label '",
         errmsg);
   labeldat_print(&arcdat->labeldat, interp->dct, PO_ERROR);
   error("' within the parent %s(%s)\n", daguid_type2str(uid_parent),
         empdag_getname(empdag, uid_parent));
   return Error_EMPIncorrectInput;
}

static NONNULL int addobjaddmaps(Interpreter *interp, mpid_t mpid, unsigned len,
                                 const double * restrict coeffs, const rhp_idx *vis)
{
   assert(len > 0);
   EmpDag *empdag = &interp->mdl->empinfo.empdag;
   MpIdArray *mps = &empdag->objfn_maps.mps;
   Lequ **lequs = empdag->objfn_maps.lequs;

   unsigned old_max = mps->max, old_len = mps->len;
   S_CHECK(mpidarray_add(mps, mpid));

   if (mps->max != old_max) {
      assert(mps->max > old_max);
      REALLOC_(lequs, Lequ *, mps->max);
      empdag->objfn_maps.lequs = lequs;
   }
   Lequ *le;
   A_CHECK(le, lequ_new_from_data(len, vis, coeffs));

   lequs[old_len] = le;

   return OK;
}

static int addarc(Interpreter *interp, daguid_t uid_parent, daguid_t uid_child,
                   ArcDat *arcdat)
{
   EmpDag *empdag = &interp->mdl->empinfo.empdag;

   trace_empdag("[empinterp] Adding edge of type %s from %s to %s\n",
                linktype2str(arcdat->type), empdag_getname(empdag, uid_parent),
                empdag_getname(empdag, uid_child));

   LinkType linktype = arcdat->type;
   EmpDagArc arc = { .type = linktype };

   switch (linktype) {
   case LinkArcVF: {

      if (!uidisMP(uid_parent) || !uidisMP(uid_child)) {
         const char errmsg[] = "a VF arc must point to an MP node";
         return err_wrong_node_type(interp, uid_parent, arcdat, errmsg);
      }

      mpid_t mpid_child = uid2id(uid_child);
      MathPrgm *mp_child;
      S_CHECK(empdag_getmpbyid(empdag, mpid_child, &mp_child));

      RhpSense sense = mp_getsense(mp_child);
      if (sense != RhpMin && sense != RhpMax) {
         const char errmsg[] = "a VF arc must point to an OPT MP node";
         return err_wrong_node_type(interp, uid_parent, arcdat, errmsg);
      }

      mpid_t mpid_parent = uid2id(uid_parent);
      MathPrgm *mp;
      S_CHECK(empdag_getmpbyid(empdag, mpid_parent, &mp));

      rhp_idx objequ = mp_getobjequ(mp);
      if (!valid_ei(objequ)) {
         MpType mptype = mp_gettype(mp);
         if (mptype == MpTypeOpt || mptype == MpTypeDual) {
            objequ = IdxObjFunc;
         }
      }
      assert(valid_ei(objequ) || (objequ == IdxCcflib && mp_gettype(mp) == MpTypeCcflib)
                              || (objequ == IdxObjFunc));

      arc.Varc.type = ArcVFBasic;
      arc.Varc.mpid_child = mpid_child;
      arc.Varc.basic_dat.ei = objequ;
      arc.Varc.basic_dat.cst = arcdat->basic_dat.cst;
      arc.Varc.basic_dat.vi = arcdat->basic_dat.vi;

      break;
   }

   case LinkArcNash:

      assert(uidisNash(uid_parent));
      if (!uidisMP(uid_child)) {
         const char *errmsg = "a Nash arc can only point to an MP node";
         return err_wrong_node_type(interp, uid_parent, arcdat, errmsg);
      }

      break;

   case LinkArcCtrl:
      /* Do nothing, we allow any MP, including VI, to have a lower level */
      break;
   case LinkVFObjFn: {
      assert(uidisMP(uid_parent));

      if (!uidisMP(uid_child)) {
         const char errmsg[] = "the 'objfn' attribute is only valid for an MP node";
         return err_wrong_node_type(interp, uid_parent, arcdat, errmsg);
      }

      mpid_t mpid_src = uid2id(uid_child), mpid_dst = uid2id(uid_parent);

      MathPrgm *mp_src;
      S_CHECK(empdag_getmpbyid(empdag, mpid_src, &mp_src));

      if (!(mp_isopt(mp_src) || mp_isccflib(mp_src))) {
         const char *errmsg = "the 'objfn' attribute is only valid for an OPT MP node";
         return err_wrong_node_type(interp, uid_parent, arcdat, errmsg);
      }

      S_CHECK(mpidarray_add(&empdag->objfn.src, mpid_src));
      S_CHECK(mpidarray_add(&empdag->objfn.dst, mpid_dst));

      return OK;
   }

   case LinkViKkt: {
      assert(uidisMP(uid_parent));

      if (!uidisMP(uid_child)) {
         const char errmsg[] = "the 'kkt' operator is only valid for an MP node";
         return err_wrong_node_type(interp, uid_parent, arcdat, errmsg);
      }

      mpid_t mpid_src = uid2id(uid_child), mpid_vi = uid2id(uid_parent);

      MathPrgm *mp_src;
      S_CHECK(empdag_getmpbyid(empdag, mpid_src, &mp_src));

      if (!(mp_isopt(mp_src) || mp_isccflib(mp_src))) {
         const char errmsg[] = "the 'kkt' operator is only valid for an OPT MP node";
         return err_wrong_node_type(interp, uid_parent, arcdat, errmsg);
      }

      S_CHECK(mpidarray_add(&empdag->fooc.src, mpid_src));
      S_CHECK(mpidarray_add(&empdag->fooc.vi, mpid_vi));

      S_CHECK(mp_operator_kkt(mp_src));

      return OK;
   }
   case LinkObjAddMap: {
      error("[empinterp] ERROR: wrong function called for link type %s",
            linktype2str(arcdat->type));
      return Error_RuntimeError;
   }

   default:
      TO_IMPLEMENT("VF edge Type not implemented");
   }


   return empdag_addarc(empdag, uid_parent, uid_child, &arc);
}

static int dag_resolve_arc_labels(Interpreter *interp)
{
   const DagRegister * restrict dagregister = &interp->dagregister;
   const LinkLabels2Arcs * restrict labels2resolve = &interp->linklabels2arcs;
   EmpDag * restrict empdag = &interp->mdl->empinfo.empdag;

   unsigned num_err = 0;

  /* ----------------------------------------------------------------------
   * First check that all basename do exists and are well defined
   *
   * TODO: we do a double check, might be quadratic in the number of labels.
   * ---------------------------------------------------------------------- */

   int status = OK;
   unsigned dagreg_len = dagregister->len;

   for (unsigned i = 0, len = labels2resolve->len; i < len; ++i) {
      LinkLabels *link = labels2resolve->arr[i];

      unsigned num_children = link->nchildren;

      if (num_children == 0) {
         error("[empinterp] ERROR: empty daglabel for node '%s'.\n",
               empdag_getname(empdag, link->daguid_parent));
         status = Error_EMPIncorrectInput;
         continue;
      }

      const char *label = link->label;
      uint16_t label_len = link->label_len;

      if (!label || label_len == 0) { continue; }

      bool found = false;

      for (unsigned j = 0; j < dagreg_len; ++j) {
         const DagRegisterEntry *entry = dagregister->list[j];
         if (entry->label_len == label_len &&
            !strncasecmp(label, entry->label, label_len)) {
            found = true;
            break;
         }
      }

      if (!found) {
         error("[empinterp] ERROR: no problem with name '%.*s' is defined\n", label_len, label);
         num_err++;
         status = Error_EMPIncorrectInput;
      }
   }

   if (status != OK) {
      error("[empinterp] %u fatal error%s while resolving labels, exiting\n", num_err, num_err > 1 ? "s" : "");
      return status;
   }

   for (unsigned i = 0, len = labels2resolve->len; i < len; ++i) {
      LinkLabels *link = labels2resolve->arr[i];
      int * restrict child = link->uels_var;
      uint8_t num_vars = link->nvaridxs;
      uint8_t dim = link->dim;
      daguid_t daguid_src = link->daguid_parent;
      assert(daguid_src != EMPDAG_UID_NONE);

      UIntArray *arcs = &empdag->mps.Carcs[i];

      unsigned num_children = link->nchildren;

      const double * restrict coeffs;
      const rhp_idx * restrict vis;
      A_CHECK(coeffs, link->coeff);
      A_CHECK(vis, link->vi);

      if (link->linktype == LinkObjAddMap) {
         assert(uidisMP(daguid_src));
         S_CHECK(addobjaddmaps(interp, uid2id(daguid_src), num_children, coeffs, vis));
         continue;
      }

      ArcDat arcdat = {
         .type = link->linktype,
         .labeldat = {.dim = link->dim, .label_len = link->label_len, .label = link->label}};

      /* Copy the fixes UELs */
      memcpy(arcdat.labeldat.uels, link->data, sizeof(int)*dim);

      /* Reserve the space for the edges */
      S_CHECK(rhp_uint_reserve(arcs, num_children));

      const int * restrict positions = &link->data[dim];
      for (unsigned j = 0, nlabels = num_children; j < nlabels; ++j, child += num_vars) {
 
         for (uint8_t k = 0; k < num_vars; ++k) {
            assert(positions[k] < dim);
            arcdat.labeldat.uels[positions[k]] = child[k];
         }

         daguid_t daguid_dst = EMPDAG_UID_NONE;

        daguid_dst = dagregister_find(dagregister, &arcdat.labeldat);

        if (daguid_dst == UINT_MAX) {
           error("[empinterp] ERROR: while building edge for node '%s' could "
                 "not resolve the label '", empdag_getname(empdag, daguid_src));
           labeldat_print(&arcdat.labeldat, interp->dct, PO_ERROR);
           errormsg("'\n");
           num_err++;
           continue;

        }

         arcdat.basic_dat.vi  = vis[j];
         arcdat.basic_dat.cst = coeffs[j];
 
         S_CHECK(addarc(interp, daguid_src, daguid_dst, &arcdat));
 
      }
   }

   if (num_err > 0) {
      error("[empinterp] during the labels resolution, %u errors were encountered!\n", num_err);
      return Error_EMPIncorrectInput;
   }

   return OK;
}

static int dag_resolve_arc_label(Interpreter *interp)
{
   const DagRegister * restrict dagregister = &interp->dagregister;
   const LinkLabel2Arc * restrict label2resolve = &interp->linklabel2arc;

   unsigned num_err = 0;

   int status = OK;
   unsigned dagreg_len = dagregister->len;

   // TODO: DELETE?

   for (unsigned i = 0, len = label2resolve->len; i < len; ++i) {
      LinkLabel *dagl = label2resolve->arr[i];

      const char *basename = dagl->label;
      uint16_t basename_len = dagl->label_len;
      bool found = false;

      for (unsigned j = 0; j < dagreg_len; ++j) {
         const DagRegisterEntry *entry = dagregister->list[j];
         if (entry->label_len == basename_len &&
            !strncasecmp(basename, entry->label, basename_len)) {
            found = true;
            break;
         }
      }

      if (!found) {
         error("[empinterp] ERROR: no problem with name '%.*s' is defined\n", basename_len, basename);
         num_err++;
         status = Error_EMPIncorrectInput;
      }
   }

   if (status != OK) {
      error("[empinterp] %u fatal error%s while resolving labels, exiting\n", num_err, num_err > 1 ? "s" : "");
      return status;
   }

   for (unsigned i = 0, len = label2resolve->len; i < len; ++i) {
      LinkLabel *link = label2resolve->arr[i]; assert(link);
      uint8_t dim = link->dim;
      daguid_t uid_parent = link->daguid_parent;
      assert(valid_linktype(link->linktype));

      ArcDat arcdat = {.type = link->linktype,  .labeldat = {.dim = link->dim,
         .label_len = link->label_len, .label = link->label} };

      memcpy(arcdat.labeldat.uels, link->uels, dim*sizeof(int));

         daguid_t uid_child = dagregister_find(dagregister, &arcdat.labeldat);

         if (uid_child == UINT_MAX) {
            errormsg("[empinterp] ERROR: could not resolve the label \"");
            labeldat_print(&arcdat.labeldat, interp->dct, PO_ERROR);
            errormsg("\"\n");
            num_err++;
            continue;
            
         }

      arcdat.basic_dat.cst = link->coeff;
      arcdat.basic_dat.vi = link->vi;
         
         S_CHECK(addarc(interp, uid_parent, uid_child, &arcdat));
      }

   if (num_err > 0) {
      error("[empinterp] during the labels resolution, %u errors were encountered!\n", num_err);
      return Error_EMPIncorrectInput;
   }

   return OK;
}

static int dag_resolve_duals_label(Interpreter *interp)
{
   const DagRegister * restrict dagregister = &interp->dagregister;
   const DualsLabelArray * restrict dualslabels = &interp->dualslabels;

   unsigned num_err = 0;

   int status = OK;
   unsigned dagreg_len = dagregister->len;

   EmpDag *empdag = &interp->mdl->empinfo.empdag;

   // TODO: DELETE?

   for (unsigned i = 0, len = dualslabels->len; i < len; ++i) {
      DualsLabel *dual = dualslabels->arr[i];

      const char *label = dual->label;
      uint16_t label_len = dual->label_len;
      bool found = false;

      for (unsigned j = 0; j < dagreg_len; ++j) {
         const DagRegisterEntry *entry = dagregister->list[j];
         if (entry->label_len == label_len &&
            !strncasecmp(label, entry->label, label_len)) {
            found = true;
            break;
         }
      }

      if (!found) {
         error("[empinterp] ERROR: no node with name '%.*s' is defined\n", label_len, label);
         num_err++;
         status = Error_EMPIncorrectInput;
      }
   }

   if (status != OK) {
      error("[empinterp] %u fatal error%s while resolving labels, exiting\n", num_err, num_err > 1 ? "s" : "");
      return status;
   }

   for (unsigned i = 0, len = dualslabels->len; i < len; ++i) {
      DualsLabel * restrict dual = dualslabels->arr[i];
      uint8_t dim = dual->dim;
      int * restrict child_uels_var = dual->uels_var;
      uint8_t num_vars = dual->nvaridxs;
      unsigned num_children = dual->mpid_duals.len;

      LabelDat labeldat = {.dim = dim, .label_len = dual->label_len, .label = dual->label};

      /* Initialize with the fixed UELs positions */
      memcpy(labeldat.uels, dual->data, sizeof(int)*dim);

      const int * restrict positions = &dual->data[dim];
      for (unsigned j = 0, nlabels = num_children; j < nlabels; ++j, child_uels_var += num_vars) {
 
         for (uint8_t k = 0; k < num_vars; ++k) {
            assert(positions[k] < dim);
            labeldat.uels[positions[k]] = child_uels_var[k];
         }


         mpid_t mpid_dual = dual->mpid_duals.arr[j];



         daguid_t daguid_parent = dagregister_find(dagregister, &labeldat);

         if (daguid_parent == UINT_MAX) {
            errormsg("[empinterp] ERROR: could not resolve the label '");
            labeldat_print(&labeldat, interp->dct, PO_ERROR);
            errormsg("'\n");
            num_err++;
            continue;

         }

         mpid_t mpid_primal = uid2id(daguid_parent);
         bool err = !uidisMP(daguid_parent);
         MathPrgm *mp; 

         if (!err) {
            S_CHECK(empdag_getmpbyid(empdag, mpid_primal, &mp));
            err = !mp || !(mp_isopt(mp) || mp_isccflib(mp));
         }

         if (err) {
            errormsg("[empinterp] ERROR: label '");
            labeldat_print(&labeldat, interp->dct, PO_ERROR);
            errormsg("' did not resolved to an MP. The dual operator can only be applied to OPT MPs\n");
            num_err++;
         }

         // HACK: we don't sync in VM mode yet
         MathPrgm *mp_dual;
         S_CHECK(empdag_getmpbyid(empdag, mpid_dual, &mp_dual));

         if (mp_dual->type != MpTypeDual) {
            error("[empinterp] ERROR: MP(%s) has type %s, expected %s",
                  mp_getname(mp_dual), mptype2str(mp_dual->type),
                  mptype2str(MpTypeDual));
            num_err++;
         }

         mp_dual->dual.dualdat = dual->opdat;

         S_CHECK(add_dualize_operation(empdag, mpid_primal, mpid_dual));
      }
   }

   if (num_err > 0) {
      error("[empinterp] during the labels resolution, %u errors were encountered!\n", num_err);
      return Error_EMPIncorrectInput;
   }

   return OK;
}

static int dag_resolve_dual_label(Interpreter *interp)
{
   const DagRegister * restrict dagregister = &interp->dagregister;
   const DualLabelArray * restrict dual_label = &interp->dual_label;

   unsigned num_err = 0;

   int status = OK;
   unsigned dagreg_len = dagregister->len;

   EmpDag *empdag = &interp->mdl->empinfo.empdag;

   // TODO: DELETE?

   for (unsigned i = 0, len = dual_label->len; i < len; ++i) {
      DualLabel *dual = dual_label->arr[i];

      const char *label = dual->label;
      uint16_t label_len = dual->label_len;
      bool found = false;

      for (unsigned j = 0; j < dagreg_len; ++j) {
         const DagRegisterEntry *entry = dagregister->list[j];
         if (entry->label_len == label_len &&
            !strncasecmp(label, entry->label, label_len)) {
            found = true;
            break;
         }
      }

      if (!found) {
         error("[empinterp] ERROR: no node with name '%.*s' is defined\n", label_len, label);
         num_err++;
         status = Error_EMPIncorrectInput;
      }
   }

   if (status != OK) {
      error("[empinterp] %u fatal error%s while resolving labels, exiting\n", num_err, num_err > 1 ? "s" : "");
      return status;
   }

   for (unsigned i = 0, len = dual_label->len; i < len; ++i) {
      DualLabel * restrict dual = dual_label->arr[i];
      uint8_t dim = dual->dim;
      mpid_t mpid_dual = dual->mpid_dual;


      LabelDat labeldat = {.dim = dim, .label_len = dual->label_len, .label = dual->label};

      memcpy(labeldat.uels, dual->uels, dim*sizeof(int));

      daguid_t daguid_parent = dagregister_find(dagregister, &labeldat);

      if (daguid_parent == UINT_MAX) {
         errormsg("[empinterp] ERROR: could not resolve the label '");
         labeldat_print(&labeldat, interp->dct, PO_ERROR);
         errormsg("'\n");
         num_err++;
         continue;

      }

      mpid_t mpid_primal = uid2id(daguid_parent);
      bool err = !uidisMP(daguid_parent);
      MathPrgm *mp; 

      if (!err) {
         S_CHECK(empdag_getmpbyid(empdag, mpid_primal, &mp));
         err = !mp || !(mp_isopt(mp) || mp_isccflib(mp));
      }

      if (err) {
         errormsg("[empinterp] ERROR: label '");
         labeldat_print(&labeldat, interp->dct, PO_ERROR);
         errormsg("' did not resolved to an MP. The dual operator can only be applied to OPT MPs\n");
      }

      S_CHECK(add_dualize_operation(empdag, mpid_primal, mpid_dual));
   }

   if (num_err > 0) {
      error("[empinterp] during the labels resolution, %u errors were encountered!\n", num_err);
      return Error_EMPIncorrectInput;
   }

   return OK;
}

int empinterp_set_empdag_root(Interpreter *interp)
{
   daguid_t root_daguid = UINT_MAX; /* silence a (buggy?) compiler warning */
   EmpDag * restrict empdag = &interp->mdl->empinfo.empdag;

   /* The root could already be set */
   if (empdag->roots.len == 1) {
      assert(is_simple_Nash(interp) || _has_single_mp(interp));
      return OK;
   }

   /* TODO CHECK that this makes sense */
   unsigned n_mps = empdag->mps.len;
   if (n_mps == 0) { return OK; }

   if (!interp->dag_root_label) {
      S_CHECK(empdag_infer_roots(empdag));

      unsigned n_roots = empdag->roots.len;
      if (n_roots > 1) {
         error("[empinterp] ERROR: %u root nodes found but EMPDAG expects "
               "only 1! List of root nodes:\n", n_roots);

         daguid_t *roots_arr = empdag->roots.arr;
         for (unsigned i = 0; i < n_roots; ++i) {
            daguid_t uid = roots_arr[i];
            error("%*c%s(%s)\n", 12, ' ', daguid_type2str(uid),
                  empdag_getname(empdag, uid));
         }

         errormsg("[empinterp] The specification of the root node is done via the 'DAG' keyword\n");
         return Error_EMPIncorrectInput;
      }

      return OK;
   }

   const DagRegister * restrict dagregister = &interp->dagregister;
   LinkLabel *dagl = interp->dag_root_label;

   ArcDat arcdat = {.type = dagl->linktype,  .labeldat = {.dim = dagl->dim,
      .label_len = dagl->label_len, .label = dagl->label} };

   root_daguid = dagregister_find(dagregister, &arcdat.labeldat);

   if (root_daguid == UINT_MAX) {
      errormsg("[empinterp] ERROR: while setting the EMPDAG root, could not "
               "resolve the label '");
      labeldat_print(&arcdat.labeldat, interp->dct, PO_ERROR);
      errormsg("'\n");
      return Error_EMPIncorrectInput;
   }

   return empdag_setroot(empdag, root_daguid);
}

int empinterp_resolve_labels(struct interpreter *interp)
{
   trace_empinterpmsg("[empinterp] Creating the arcs of the EMPDAG.\n");
   S_CHECK(dag_resolve_arc_labels(interp));
   S_CHECK(dag_resolve_arc_label(interp));
   S_CHECK(dag_resolve_dual_label(interp));
   S_CHECK(dag_resolve_duals_label(interp));
//   S_CHECK(dag_resolve_fooc_label(interp));

   return OK;
}
