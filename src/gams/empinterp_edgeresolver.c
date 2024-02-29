#include "asprintf.h"

#include "empdag.h"
#include "empdag_common.h"
#include "empinfo.h"
#include "empinterp.h"
#include "empinterp_edgeresolver.h"
#include "empinterp_priv.h"
#include "empinterp_utils.h"
#include "mathprgm.h"
#include "mdl.h"
#include "printout.h"
#include "status.h"

typedef struct problem_edge {
   ArcType type;
   uint8_t dim;
   uint16_t basename_len;
   const char *basename;
   int uels[GMS_MAX_INDEX_DIM];
} ProblemEdge;


static void problem_edge_print(ProblemEdge *pe, dctHandle_t dct, unsigned mode)
{
   printout(mode, "%.*s", pe->basename_len, pe->basename);
   unsigned dim = pe->dim;

   if (dim == 0) return;

   printstr(mode, "(");
   int dummyoffset;
   dct_printuel(dct, pe->uels[0], mode, &dummyoffset);


   for (unsigned i = 1; i < dim; ++i) {
      printstr(mode, ", ");
      dct_printuel(dct, pe->uels[i], mode, &dummyoffset);
   }

   printstr(mode, ")");
}

static daguid_t dagregister_find(const DagRegister *dagreg, ProblemEdge *edge)
{
   const char * restrict basename = edge->basename;
   uint16_t basename_len = edge->basename_len;
   uint8_t edge_dim = edge->dim;
   int *uels = edge->uels;

   for (unsigned i = 0, len = dagreg->len; i < len; ++i) {
      const DagRegisterEntry *entry = dagreg->list[i];

      if (entry->basename_len != basename_len ||
          entry->dim != edge_dim ||
          strncasecmp(basename, entry->basename, basename_len) != 0) {
         goto _loop;
      }

      for (unsigned j = 0, dim = entry->dim; j < dim; ++j) {
         if (uels[j] != entry->uels[j]) goto _loop;
      }

      return entry->daguid;
_loop:
      ;
   }

   return UINT_MAX;
}

static int addedge(Interpreter *interp, daguid_t daguid, daguid_t child_daguid,
                   ProblemEdge *edge)
{
   EmpDag *empdag = &interp->mdl->empinfo.empdag;

   trace_empdag("[empinterp] Adding edge of type %s from %s to %s\n",
                arctype_str(edge->type), empdag_getname(empdag, daguid),
                empdag_getname(empdag, child_daguid));

   EmpDagArc empedge = { .type = edge->type };
   if (edge->type == ArcVF) {

      if (!uidisMP(daguid)) {
         errormsg("[empinterp] ERROR: a VF edge can only point to an MP node. "
                  "This error happened while processing the label '");
         problem_edge_print(edge, interp->dct, PO_ERROR);
         error("' with parent '%s'\n", empdag_getname(empdag, daguid));
         return Error_EMPIncorrectSyntax;
      }

      unsigned mpchild_id = uid2id(child_daguid);
      MathPrgm *mp_child;
      S_CHECK(empdag_getmpbyid(empdag, mpchild_id, &mp_child));

      RhpSense sense = mp_getsense(mp_child);
      if (sense != RhpMin && sense != RhpMax) {
         errormsg("[empinterp] ERROR: a VF edge can only point to an OPT MP node. "
                  "This error happened while processing the label '");
         problem_edge_print(edge, interp->dct, PO_ERROR);
         error("' with parent '%s'\n", empdag_getname(empdag, daguid));
         return Error_EMPIncorrectSyntax;
      }

      mpid_t parent_id = uid2id(daguid);
      MathPrgm *mp;
      S_CHECK(empdag_getmpbyid(empdag, parent_id, &mp));
      rhp_idx objequ = mp_getobjequ(mp);

      if (mp_gettype(mp) == MpTypeOpt && !valid_ei(objequ)) {
         error("[empinterp] ERROR: while building a %s edge for the OPT MP "
               "node '%s' has no objective function.\n", arctype_str(edge->type),
               empdag_getname(empdag, daguid));
         return Error_EMPIncorrectSyntax;
      }
      assert(valid_ei(objequ) || (objequ == IdxCcflib && mp_gettype(mp) == MpTypeCcflib));

      empedge.Varc.type = ArcVFBasic;
      empedge.Varc.child_id = mpchild_id;
      empedge.Varc.basic_dat.ei = objequ;
      empedge.Varc.basic_dat.cst = 1.;
      empedge.Varc.basic_dat.vi = IdxNA;

   } else if (edge->type == ArcNash) {

      assert(uidisMPE(daguid));
      if (!uidisMP(child_daguid)) {
         errormsg("[empinterp] ERROR: a Nash edge can only point to an MP node. "
                  "This error happened while processing the label '");
         problem_edge_print(edge, interp->dct, PO_ERROR);
         error("' with parent '%s'\n", empdag_getname(empdag, daguid));
         return Error_EMPIncorrectSyntax;
      }

   } else if (edge->type == ArcCtrl) {
      /* Do nothing, we allow any MP, including VI, to have a lower level */
   } else {
      TO_IMPLEMENT("VF edge Type not implemented");
   }


   return empdag_addarc(empdag, daguid, child_daguid, &empedge);
}

static int dag_resolve_labels(Interpreter *interp)
{

   const DagRegister * restrict dagregister = &interp->dagregister;
   const DagLabels2Edges * restrict labels2resolve = &interp->labels2edges;
   EmpDag * restrict empdag = &interp->mdl->empinfo.empdag;

   unsigned num_err = 0;

   for (unsigned i = 0, len = labels2resolve->len; i < len; ++i) {
      DagLabels *dagl = labels2resolve->list[i];
      int * restrict child = dagl->uels_var;
      uint8_t num_vars = dagl->num_var;
      uint8_t dim = dagl->dim;
      daguid_t daguid = dagl->daguid;
      assert(daguid != EMPDAG_UID_NONE);

      unsigned id = uid2id(daguid);
      UIntArray *arcs = &empdag->mps.Carcs[i];

      unsigned num_children = dagl->num_children;

      if (num_children == 0) {
         error("[empinterp] ERROR: empty daglabel for node '%s'.\n",
               empdag_getname(empdag, dagl->daguid));
         continue;
      }

      ProblemEdge edge = {.type = dagl->arc_type,  .dim = dagl->dim,
         .basename_len = dagl->basename_len, .basename = dagl->basename};
      memcpy(edge.uels, dagl->data, sizeof(int)*dim);

      /* Reserve the space for the edges */
      S_CHECK(rhp_uint_reserve(arcs, num_children));

      const int * restrict positions = &dagl->data[dim];
      for (unsigned j = 0, nlabels = num_children; j < nlabels; ++j, child += num_vars) {
         
         for (uint8_t k = 0; k < num_vars; ++k) {
            assert(positions[k] < dim);
            edge.uels[positions[k]] = child[k];
         }

         daguid_t child_daguid = dagregister_find(dagregister, &edge);

         if (child_daguid == UINT_MAX) {
            error("[empinterp] ERROR: while building edge for node '%s' could "
                  "not resolve the label '", empdag_getname(empdag, daguid));
            problem_edge_print(&edge, interp->dct, PO_ERROR);
            errormsg("'\n");
            num_err++;
            continue;
            
         }

         S_CHECK(addedge(interp, daguid, child_daguid, &edge));
         
      }
   }

   if (num_err > 0) {
      error("[empinterp] during the labels resolution, %u errors were encountered!\n", num_err);
      return Error_EMPIncorrectInput;
   }

   return OK;
}

static int dag_resolve_label(Interpreter *interp)
{

   const DagRegister * restrict dagregister = &interp->dagregister;
   const DagLabel2Edge * restrict label2resolve = &interp->label2edge;
   EmpDag * restrict empdag = &interp->mdl->empinfo.empdag;

   unsigned num_err = 0;

   for (unsigned i = 0, len = label2resolve->len; i < len; ++i) {
      DagLabel *dagl = label2resolve->list[i];
      uint8_t dim = dagl->dim;
      daguid_t daguid = dagl->daguid;
      unsigned id = uid2id(daguid);
      UIntArray *arcs = &empdag->mps.Carcs[i];


      ProblemEdge edge = {.type = dagl->arc_type,  .dim = dagl->dim,
         .basename_len = dagl->basename_len, .basename = dagl->basename};

      memcpy(edge.uels, dagl->uels, dim*sizeof(int));

         daguid_t child_daguid = dagregister_find(dagregister, &edge);

         if (child_daguid == UINT_MAX) {
            errormsg("[empinterp] ERROR: could not resolve the label \"");
            problem_edge_print(&edge, interp->dct, PO_ERROR);
            errormsg("\"\n");
            num_err++;
            continue;
            
         }
         
         S_CHECK(addedge(interp, daguid, child_daguid, &edge));
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
   unsigned n_mps = empdag->mps.len, n_mpes = empdag->mpes.len;
   if (n_mps == 0 && n_mpes == 0) { return OK; }

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

   ProblemEdge edge = {.type = dagl->arc_type,  .dim = dagl->dim,
      .basename_len = dagl->basename_len, .basename = dagl->basename};

   root_daguid = dagregister_find(dagregister, &edge);

   if (root_daguid == UINT_MAX) {
      errormsg("[empinterp] ERROR: while setting the EMPDAG root, could not "
               "resolve the label '");
      problem_edge_print(&edge, interp->dct, PO_ERROR);
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

int empinterp_resolve_empdag_edges(struct interpreter *interp)
{
   trace_empinterpmsg("[empinterp] Creating the edges of the EMPDAG.\n");
   S_CHECK(dag_resolve_labels(interp));
   S_CHECK(dag_resolve_label(interp));

   return OK;
}
