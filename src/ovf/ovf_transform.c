#include "asprintf.h"

#include <stdio.h>
#include <string.h>

#include "container.h"
#include "empinfo.h"
#include "macros.h"
#include "mdl.h"
#include "mdl_rhp.h"
#include "ovf_conjugate.h"
#include "ovf_equil.h"
#include "ovf_fenchel.h"
#include "ovf_options.h"
#include "ovfinfo.h"
#include "printout.h"
#include "ovf_transform.h"
#include "reshop_data.h"
#include "rhp_graph.h"
#include "rhp_search.h"
#include "status.h"
#include "var.h"


#ifndef NDEBUG
#define OVF_DEBUG(str, ...) trace_process("[OVF] DEBUG: " str "\n", __VA_ARGS__)
#else
#define OVF_DEBUG(...)
#endif


struct sort_obj {
  size_t mp_id;
  rhp_idx vi;
  void *obj;
};


#define SORT_NAME rhpobj
#define SORT_TYPE struct sort_obj
#define SORT_CMP(x, y) ((x).vi - (y).vi)
#include "sort.h"

/**
 * @brief Perform all the OVF transformations
 *
 * @param mdl  the model
 *
 * @return     the error code
 */
int ovf_transform(Model *mdl)
{
   int status = OK;
   Container *ctr = &mdl->ctr;
   EmpInfo *empinfo = &mdl->empinfo;
   ObjArray topo_order;
   rhp_obj_init(&topo_order);

   S_CHECK(rmdl_incstage(mdl));

   struct ovfinfo *ovfinfo = empinfo->ovf;
   OvfDef *ovf = ovfinfo->ovf_def;

   /* ----------------------------------------------------------------------
    * We need to sort the OVF functions before performing the reformulation.
    * This should lead to better performances, but also nicer forms.
    * The process is as follows:
    * - Get the number of OVF
    * - Sort them using a dependency resolution via topological sort
    * - Reformulation them in that order
    * ---------------------------------------------------------------------- */

   /* TODO(xhub) low this could be avoided if we create this in a better way  */
   size_t n_ovfs = 0;
   OvfDef **ovfs = NULL;
   struct sort_obj *ovf_objs = NULL;
   rhp_idx *ovf_vidxs = NULL;
   struct rhp_graph_gen **ovf_nodes = NULL;
   struct rhp_graph_child *node_children = NULL;
   bool *node_children_ovf_stored = NULL;

   /*  TODO(xhub) that's a lot of MALLOCs */
   MALLOC_EXIT(ovfs, OvfDef *, ovfinfo->num_ovf);
   CALLOC_EXIT(ovf_objs, struct sort_obj, ovfinfo->num_ovf);
   MALLOC_EXIT(ovf_vidxs, rhp_idx, ovfinfo->num_ovf);
   MALLOC_EXIT(ovf_nodes, struct rhp_graph_gen *, ovfinfo->num_ovf);
   MALLOC_EXIT(node_children, struct rhp_graph_child, ovfinfo->num_ovf);
   MALLOC_EXIT(node_children_ovf_stored, bool, ovfinfo->num_ovf);

   while (ovf) {
      struct sort_obj *o = &ovf_objs[n_ovfs];
      o->mp_id = n_ovfs;          /* Set counter for this OVF */
      o->vi = ovf->ovf_vidx;     /* Store the OVF vidx       */
      o->obj = rhp_graph_gen_alloc(ovf);
      ovf_nodes[n_ovfs] = o->obj;
      ovfs[n_ovfs] = ovf;
      n_ovfs++;
      ovf = ovf->next;
   }

   assert(n_ovfs == ovfinfo->num_ovf);

   /* ----------------------------------------------------------------------
    * Sort OVFs by indices, it makes it easier to test if a variable is OVF
    * Also keep a vector of the OVF variables, it makes it faster to test for
    * membership
    * ---------------------------------------------------------------------- */

   rhpobj_tim_sort(ovf_objs, n_ovfs);

   for (size_t i = 0; i < n_ovfs; ++i) {
      ovf_vidxs[i] = ovf_objs[i].vi;
   }


   /* ----------------------------------------------------------------------
    * For each OVF variable, we go through its arguments and see whether other
    * OVF variables are in it. 
    *
    * ---------------------------------------------------------------------- */
   for (size_t i = 0; i < n_ovfs; ++i) {

     ovf = ovfs[i];

   /* ----------------------------------------------------------------------
    * Reset the variables used for storage
    * ---------------------------------------------------------------------- */

     unsigned pos = 0;
     memset(node_children, 0, n_ovfs * sizeof(*node_children));
     memset(node_children_ovf_stored, 0, n_ovfs * sizeof(bool));

     unsigned n_args = avar_size(ovf->args);

      for (size_t j = 0; j < n_args; ++j) {
         rhp_idx vi_arg = avar_fget(ovf->args, j);

         if (!valid_vi(vi_arg)) {
            error("%s :: invalid argument index %u", __func__, vi_arg);
            status = Error_RuntimeError;
            goto _exit;
         }

        void *jacptr = NULL;
        double jacval;
        rhp_idx ei;
        int nlflag;

        /* ------------------------------------------------------------------
         * Check if in the equations used as arguments for the OVF variable
         * contains other OVF vars. If yes, put all these OVFs as children
         * ------------------------------------------------------------------ */

        do {
          S_CHECK_EXIT(ctr_var_iterequs(ctr, vi_arg, &jacptr, &jacval, &ei, &nlflag));

          if (!valid_ei(ei)) {
            error("%s :: Invalid equation for OVF argument variable %s of OVF "
                  "variable %s.\n", __func__, ctr_printvarname(ctr, vi_arg),
                  ctr_printvarname2(ctr, ovf->ovf_vidx));
            status = Error_InvalidValue;
            goto _exit;
          }

            OVF_DEBUG("OVF argvar %s is present in equation %s",
                      ctr_printvarname(ctr, vi_arg), ctr_printequname(ctr, ei))

          /* Now explore ei */
          void *jacptr_vi = NULL;
          double jacval_vi;
          rhp_idx vi;
          int nlflag_vi;

          do {
            S_CHECK_EXIT(ctr_equ_itervars(ctr, ei, &jacptr_vi, &jacval_vi, &vi, &nlflag_vi));

            if (!valid_vi(vi)) {
              error("%s :: invalid variables for equation %s (#%d).\n", __func__,
                    ctr_printequname(ctr, ei), ei);
              status = Error_InvalidValue;
              goto _exit;
            }

            /* -------------------------------------------------------------
             * perform the search and insert the node in the children list,
             * only if it hasn't been added earlier
             * ------------------------------------------------------------- */

            unsigned idx = bin_search_idx(ovf_vidxs, n_ovfs, vi);
            if (idx < n_ovfs && !node_children_ovf_stored[idx]) {
                  OVF_DEBUG("OVF var %s is present in equation %s",
                            ctr_printvarname(ctr, vi), ctr_printequname(ctr, ei))
              node_children[pos].idx = vi_arg;
              node_children[pos++].child = (struct rhp_graph_gen *)ovf_objs[idx].obj;
              node_children_ovf_stored[idx] = true;
            }

          } while (jacptr_vi);

        } while (jacptr);

      } /* END: for loop on OVF arguments  */

     /* ---------------------------------------------------------------------
      * Insert any children
      * --------------------------------------------------------------------- */

     if (pos > 0) {
       S_CHECK_EXIT(rhp_graph_gen_set_children(ovf_nodes[i], node_children, pos));
     }

   } /* END: for loop on OVF  */

   /* ----------------------------------------------------------------------
    * Now the dependency graph of the OVF variables has been built, perform
    * a topological sort and get going with the reformulations
    * ---------------------------------------------------------------------- */

   status = rhp_graph_gen_dfs(ovf_nodes, ovfinfo->num_ovf, &topo_order);

   const char *ovfgraph_dir = mygetenv("RHP_EXPORT_OVFGRAPH");
   if (ovfgraph_dir) {
      char *fname;
      IO_CALL_EXIT(asprintf(&fname, "%s" DIRSEP "ovf_%p.dot", ovfgraph_dir,
                       (void*)ovfinfo));
      status = ovfgraph_dot(ovfinfo, ovf_nodes, n_ovfs, mdl, fname);
      FREE(fname);
      if (status != OK) goto _exit;
   }
   myfreeenvval(ovfgraph_dir);

   if (status) goto _exit;

   /* ----------------------------------------------------------------------
    * Synchronize with environment
    * ---------------------------------------------------------------------- */
   S_CHECK_EXIT(optset_syncenv(&ovf_optset));

   /* ----------------------------------------------------------------------
    * Now we can iterate on all OVF in the right order
    * ---------------------------------------------------------------------- */

   for (size_t i = 0; i < ovfinfo->num_ovf; ++i) {
      ovf = (OvfDef *)topo_order.arr[i];

      S_CHECK_EXIT(ovf_finalize(mdl, ovf));

      printout(PO_V, "[OVF] Reformulating OVF variable %s (#%d) with scheme ",
               ctr_printvarname(ctr, ovf->ovf_vidx), ovf->ovf_vidx);

      enum ovf_scheme reformulation;
      if (ovf->reformulation != OVF_Scheme_Unset) {
         reformulation = ovf->reformulation;
      } else {
         reformulation = O_Ovf_Reformulation;
      }


      switch (reformulation) {
      case OVF_Equilibrium:
         printout(PO_V, "equilibrium\n");
         S_CHECK_EXIT(ovf_equil(mdl, OVFTYPE_OVF,
                          (union ovf_ops_data){.ovf =  ovf}));
         break;
      case OVF_Fenchel:
         printout(PO_V, "Fenchel duality\n");
         S_CHECK_EXIT(ovf_fenchel(mdl, OVFTYPE_OVF,
                           (union ovf_ops_data){.ovf =  ovf}));
         break;
      case OVF_Conjugate:
         printout(PO_V, "conjugate duality\n");
         S_CHECK_EXIT(ovf_conjugate(mdl, OVFTYPE_OVF,
                              (union ovf_ops_data){.ovf =  ovf}));
         break;
      default:
         error("\r[OVF] ERROR: unsupported reformualtion %s\n",
                  ovf_getreformulationstr(reformulation));
         status = Error_NotImplemented;
         goto _exit;
      }
   }

_exit:
   rhp_obj_empty(&topo_order);

   FREE(ovfs);
   unsigned len = ovf_objs ? n_ovfs : 0;
   for (unsigned i = 0; i < len; ++i) {
      rhp_graph_gen_free(ovf_nodes[i]);
   }
   for (unsigned i = 0; i < len; ++i) {
      FREE(ovf_objs[i].obj);
   }

   FREE(ovf_objs);
   FREE(ovf_vidxs);
   FREE(ovf_nodes);
   FREE(node_children);
   FREE(node_children_ovf_stored);

   return status;
}
