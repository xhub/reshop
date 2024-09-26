#include "asprintf.h"

#include "empdag.h"
#include "empdag_common.h"
#include "empinfo.h"
#include "empinterp.h"
#include "empinterp_edgeresolver.h"
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
   uint16_t basename_len;
   const char *basename;
   int uels[GMS_MAX_INDEX_DIM];
} LabelDat;

typedef struct arc_dat {
   LabelDat labeldat;
   ArcType type;
} ArcDat;


static void labeldat_print(LabelDat *label, dctHandle_t dct, unsigned mode)
{
   printout(mode, "%.*s", label->basename_len, label->basename);
   unsigned dim = label->dim;

   if (dim == 0) return;

   printstr(mode, "(");
   int dummyoffset;
   dct_printuel(dct, label->uels[0], mode, &dummyoffset);


   for (unsigned i = 1; i < dim; ++i) {
      printstr(mode, ", ");
      dct_printuel(dct, label->uels[i], mode, &dummyoffset);
   }

   printstr(mode, ")");
}

static daguid_t dagregister_find(const DagRegister *dagreg, LabelDat *label)
{
   const char * restrict basename = label->basename;
   uint16_t basename_len = label->basename_len;
   uint8_t edge_dim = label->dim;
   int *uels = label->uels;

   for (unsigned i = 0, len = dagreg->len; i < len; ++i) {
      const DagRegisterEntry *entry = dagreg->list[i];

      if (entry->nodename_len != basename_len ||
          entry->dim != edge_dim ||
          strncasecmp(basename, entry->nodename, basename_len) != 0) {
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
   S_CHECK(empdag_getmpbyuid(empdag, mpid_primal, &mp_primal));
   mp_hide(mp_primal);

   const char *name_primal = empdag_getmpname(empdag, mpid_primal);
   IO_CALL(asprintf(&name_dual, "%s_dual", name_primal));
   S_CHECK(empdag_setmpname(empdag, mpid_dual, name_dual));

   S_CHECK(empdag_getmpbyid(empdag, mpid_dual, &mp_dual));

   mp_dual->dual.mpid = mpid_primal;

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

   return OK;
}

//static int getfooc(EmpDag *empdag, daguid_t daguid_dst, daguid_t *daguid_fooc)
//{
//   return OK;
//}

static int addarc(Interpreter *interp, daguid_t uid_parent, daguid_t uid_child,
                   ArcDat *arcdat)
{
   EmpDag *empdag = &interp->mdl->empinfo.empdag;

   trace_empdag("[empinterp] Adding edge of type %s from %s to %s\n",
                arctype_str(arcdat->type), empdag_getname(empdag, uid_parent),
                empdag_getname(empdag, uid_child));

   EmpDagArc arc = { .type = arcdat->type };
   if (arcdat->type == ArcVF) {

      if (!uidisMP(uid_parent) || !uidisMP(uid_child)) {
         errormsg("[empinterp] ERROR: a VF edge can only point to an MP node. "
                  "This error happened while processing the label '");
         labeldat_print(&arcdat->labeldat, interp->dct, PO_ERROR);
         error("' with parent '%s'\n", empdag_getname(empdag, uid_parent));
         return Error_EMPIncorrectSyntax;
      }

      mpid_t child_mpid = uid2id(uid_child);
      MathPrgm *mp_child;
      S_CHECK(empdag_getmpbyid(empdag, child_mpid, &mp_child));

      RhpSense sense = mp_getsense(mp_child);
      if (sense != RhpMin && sense != RhpMax) {
         errormsg("[empinterp] ERROR: a VF edge can only point to an OPT MP node. "
                  "This error happened while processing the label '");
         labeldat_print(&arcdat->labeldat, interp->dct, PO_ERROR);
         error("' with parent '%s'\n", empdag_getname(empdag, uid_parent));
         return Error_EMPIncorrectSyntax;
      }

      mpid_t parent_mpid = uid2id(uid_parent);
      MathPrgm *mp;
      S_CHECK(empdag_getmpbyid(empdag, parent_mpid, &mp));

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
      arc.Varc.mpid_child = child_mpid;
      arc.Varc.basic_dat.ei = objequ;
      arc.Varc.basic_dat.cst = 1.;
      arc.Varc.basic_dat.vi = IdxNA;

   } else if (arcdat->type == ArcNash) {

      assert(uidisNash(uid_parent));
      if (!uidisMP(uid_child)) {
         errormsg("[empinterp] ERROR: a Nash edge can only point to an MP node. "
                  "This error happened while processing the label '");
         labeldat_print(&arcdat->labeldat, interp->dct, PO_ERROR);
         error("' with parent '%s'\n", empdag_getname(empdag, uid_parent));
         return Error_EMPIncorrectSyntax;
      }

   } else if (arcdat->type == ArcCtrl) {
      /* Do nothing, we allow any MP, including VI, to have a lower level */
   } else {
      TO_IMPLEMENT("VF edge Type not implemented");
   }


   return empdag_addarc(empdag, uid_parent, uid_child, &arc);
}

static int dag_resolve_arc_labels(Interpreter *interp)
{
   const DagRegister * restrict dagregister = &interp->dagregister;
   const DagLabels2Arcs * restrict labels2resolve = &interp->labels2arcs;
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
      DagLabels *dagl = labels2resolve->arr[i];

      unsigned num_children = dagl->num_children;

      if (num_children == 0) {
         error("[empinterp] ERROR: empty daglabel for node '%s'.\n",
               empdag_getname(empdag, dagl->daguid_parent));
         status = Error_EMPIncorrectInput;
         continue;
      }

      const char *basename = dagl->nodename;
      uint16_t basename_len = dagl->nodename_len;
      bool found = false;

      for (unsigned j = 0; j < dagreg_len; ++j) {
         const DagRegisterEntry *entry = dagregister->list[j];
         if (entry->nodename_len == basename_len &&
            !strncasecmp(basename, entry->nodename, basename_len)) {
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

   for (unsigned i = 0, len = labels2resolve->len; i < len; ++i) {
      DagLabels *dagl = labels2resolve->arr[i];
      int * restrict child = dagl->uels_var;
      uint8_t num_vars = dagl->num_var;
      uint8_t dim = dagl->dim;
      daguid_t daguid_src = dagl->daguid_parent;
      assert(daguid_src != EMPDAG_UID_NONE);

      UIntArray *arcs = &empdag->mps.Carcs[i];

      unsigned num_children = dagl->num_children;

      ArcDat arcdat = {.type = dagl->arc_type, 
         .labeldat = {.dim = dagl->dim,
         .basename_len = dagl->nodename_len, .basename = dagl->nodename} };
      memcpy(arcdat.labeldat.uels, dagl->data, sizeof(int)*dim);

      /* Reserve the space for the edges */
      S_CHECK(rhp_uint_reserve(arcs, num_children));

      const int * restrict positions = &dagl->data[dim];
      for (unsigned j = 0, nlabels = num_children; j < nlabels; ++j, child += num_vars) {
 
         for (uint8_t k = 0; k < num_vars; ++k) {
            assert(positions[k] < dim);
            arcdat.labeldat.uels[positions[k]] = child[k];
         }

         daguid_t daguid_dst = dagregister_find(dagregister, &arcdat.labeldat);

         if (daguid_dst == UINT_MAX) {
            error("[empinterp] ERROR: while building edge for node '%s' could "
                  "not resolve the label '", empdag_getname(empdag, daguid_src));
            labeldat_print(&arcdat.labeldat, interp->dct, PO_ERROR);
            errormsg("'\n");
            num_err++;
            continue;
 
         }
 
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
   const DagLabel2Arc * restrict label2resolve = &interp->label2arc;

   unsigned num_err = 0;

   int status = OK;
   unsigned dagreg_len = dagregister->len;

   // TODO: DELETE?

   for (unsigned i = 0, len = label2resolve->len; i < len; ++i) {
      DagLabel *dagl = label2resolve->arr[i];

      const char *basename = dagl->nodename;
      uint16_t basename_len = dagl->nodename_len;
      bool found = false;

      for (unsigned j = 0; j < dagreg_len; ++j) {
         const DagRegisterEntry *entry = dagregister->list[j];
         if (entry->nodename_len == basename_len &&
            !strncasecmp(basename, entry->nodename, basename_len)) {
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
      DagLabel *dagl = label2resolve->arr[i];
      uint8_t dim = dagl->dim;
      daguid_t daguid = dagl->daguid_parent;


      ArcDat arcdat = {.type = dagl->arc_type,  .labeldat = {.dim = dagl->dim,
         .basename_len = dagl->nodename_len, .basename = dagl->nodename} };

      memcpy(arcdat.labeldat.uels, dagl->uels, dim*sizeof(int));

         daguid_t child_daguid = dagregister_find(dagregister, &arcdat.labeldat);

         if (child_daguid == UINT_MAX) {
            errormsg("[empinterp] ERROR: could not resolve the label \"");
            labeldat_print(&arcdat.labeldat, interp->dct, PO_ERROR);
            errormsg("\"\n");
            num_err++;
            continue;
            
         }
         
         S_CHECK(addarc(interp, daguid, child_daguid, &arcdat));
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

      const char *basename = dual->label;
      uint16_t basename_len = dual->label_len;
      bool found = false;

      for (unsigned j = 0; j < dagreg_len; ++j) {
         const DagRegisterEntry *entry = dagregister->list[j];
         if (entry->nodename_len == basename_len &&
            !strncasecmp(basename, entry->nodename, basename_len)) {
            found = true;
            break;
         }
      }

      if (!found) {
         error("[empinterp] ERROR: no node with name '%.*s' is defined\n", basename_len, basename);
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


      LabelDat labeldat = {.dim = dim, .basename_len = dual->label_len, .basename = dual->label};

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
   int status = OK;
   daguid_t root_daguid = UINT_MAX; /* silence a (buggy?) compiler warning */
   EmpDag * restrict empdag = &interp->mdl->empinfo.empdag;

   /* The root could already be set */
   if (empdag->roots.len == 1) {
      assert(is_simple_Nash(interp) || _has_single_mp(interp));
      return OK;
   }

   /* TODO CHECK that this makes sense */
   unsigned n_mps = empdag->mps.len, n_mpes = empdag->nashs.len;
   if (n_mps == 0) { return OK; }

   UIntArray roots;
   S_CHECK_EXIT(empdag_collectroots(empdag, &roots));

   unsigned n_roots = roots.len;
   if (roots.len == 1) {
      root_daguid = roots.arr[0];
      goto _add_root;
   }

   if (!interp->dag_root_label) {
      if (n_roots == 0) {
         error("[empinterp] ERROR: no root node found but EMPDAG has %u MPs "
               "and %u MPEs\n", n_mps, n_mpes);
      } else {
         error("[empinterp] ERROR: %u root nodes found but EMPDAG expects "
               "only 1! List of root nodes:\n", n_roots);
         for (unsigned i = 0; i < n_roots; ++i) {
            daguid_t uid = roots.arr[i];
            const char *nodetype = uidisMP(uid) ? "MP" : "MPE";
            error("%*c %s(%s)\n", 12, ' ', nodetype, empdag_getname(empdag, uid));
         }
      }
      errormsg("[empinterp] The specification of the root node is done via the 'DAG' keyword\n");
      status = Error_EMPIncorrectInput;
      goto _exit;
   }

   const DagRegister * restrict dagregister = &interp->dagregister;
   DagLabel *dagl = interp->dag_root_label;

   ArcDat arcdat = {.type = dagl->arc_type,  .labeldat = {.dim = dagl->dim,
      .basename_len = dagl->nodename_len, .basename = dagl->nodename} };

   root_daguid = dagregister_find(dagregister, &arcdat.labeldat);

   if (root_daguid == UINT_MAX) {
      errormsg("[empinterp] ERROR: while setting the EMPDAG root, could not "
               "resolve the label '");
      labeldat_print(&arcdat.labeldat, interp->dct, PO_ERROR);
      errormsg("'\n");
      status = Error_EMPIncorrectInput;
   }

_exit:
   rhp_uint_empty(&roots);

   if (status != OK) { return status; }

_add_root:
   rhp_uint_empty(&roots);

   trace_empinterp("[empinterp] setting %s(%s) as EMPDAG root\n",
                   daguid_type2str(root_daguid),
                   empdag_getname(empdag, root_daguid));

   return empdag_setroot(empdag, root_daguid);
}

int empinterp_resolve_labels(struct interpreter *interp)
{
   trace_empinterpmsg("[empinterp] Creating the arcs of the EMPDAG.\n");
   S_CHECK(dag_resolve_arc_labels(interp));
   S_CHECK(dag_resolve_arc_label(interp));
   S_CHECK(dag_resolve_dual_label(interp));
//   S_CHECK(dag_resolve_fooc_label(interp));

   return OK;
}
