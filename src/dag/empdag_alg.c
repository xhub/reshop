#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#include "compat.h"
#include "ctr_rhp.h"
#include "empdag.h"
#include "empdag_alg.h"
#include "empdag_uid.h"
#include "equvar_metadata.h"
#include "filter_ops.h"
#include "macros.h"
#include "mathprgm.h"
#include "mdl.h"
#include "mdl_rhp.h"
#include "mdl_timings.h"
#include "printout.h"
#include "rhp_graph_data.h"
#include "status.h"
#include "timings.h"
#include "toplayer_utils.h"


typedef unsigned nidx_t;



enum EmpDagPpty {
  EmpDagPptyNone          = 0,
  EmpDagPptyHasMpCtrl     = 1,
  EmpDagPptyHasEquilCtrl  = 2,
  EmpDagPptyHasEquil      = 4,
};

enum {
   DagErrDagCycle = 1,
   DagErrMpeNoChild = 2,
   DagErrGeneric = 3,
   DagErrLen = 4,
};


// TODO cleanup 
//static const enum EmpDagPpty EmpDagPptyHasCtrl =
//                       EmpDagPptyHasMpCtrl | EmpDagPptyHasEquilCtrl;

struct empdag_info {
  enum EmpDagPpty root;
  enum EmpDagPpty children;
};

struct sort_obj {
  mpid_t mp_id;
  rhp_idx vi;
};


#define SORT_NAME empdag_sort
#define SORT_TYPE struct sort_obj
#define SORT_CMP(x, y) ((x).vi - (y).vi)
#include "sort.h"

/* ---------------------------------------------------------------------
 * This uses a smart way to define consistent error messages that are
 * really constants. With ELF, the string are stored ina different section
 * and then the pointers needs to be updated because of the relocation.
 * With this format, any data structure is put in .rodata!
 * --------------------------------------------------------------------- */

#define DEFINE_STR() \
ERRSTR(DagErrDagCycle, "Cycle detected in the EMP DAG") \
ERRSTR(DagErrGeneric, "Generic Error while analyzing the EMP DAG") \


#define ERRSTR(nb, str) const char nb[sizeof(str)];
typedef union {
   struct {
   DEFINE_STR()
   };
   char dummystr[1];
} EmpDagErrs;
#undef ERRSTR

#define ERRSTR(nb, str) .nb = str,
const EmpDagErrs empdag_errs = {
DEFINE_STR()
};
#undef ERRSTR

#define ERRSTR(nb, str) offsetof(EmpDagErrs, nb),
const unsigned empdag_errs_offset[] = {
DEFINE_STR()
};
#undef ERRSTR

const unsigned char n_empdag_errs = sizeof(empdag_errs_offset)/sizeof(empdag_errs_offset[0]);

static inline const char* empdag_errstr(int rc)
{
   /* rc = 0 is not an error; the logical errors start at -1*/
   if (rc >= 0 || rc < -n_empdag_errs) {
      return "buggy empdag error code";
   }

   return empdag_errs.dummystr + empdag_errs_offset[-(rc+1)];
}

/* We cannot use an enum here as UINT_MAX doesn't fit in it */
#define  NodeIdxDAG       UINT_MAX
#define  NodeIdxNoParent  (UINT_MAX-1)
#define  NodeIdxError     (UINT_MAX-2)
#define  NodeIdxMaxValid  (UINT_MAX-10)

#define  valid_nidx(ni)   ((ni) < NodeIdxMaxValid)

/* TODO: is this needed? */
typedef struct {
   daguid_t root_uid;
   RhpSense root_sense;
   UIntArray adversarial_mps;
} DagSaddlePath;

typedef struct empdag_mp_ppty {
   unsigned num_ownvar;
   unsigned num_history;
   unsigned num_nashvar;
   unsigned num_ctrlvar;
   unsigned num_solvar;
   unsigned num_sharedvar;

   unsigned level;
} DagMpPpty;

typedef struct empdag_mpe_ppty {
   unsigned level;
} DagMpePpty;

typedef struct empdag_dfs {
   const Model *mdl;
   EmpDag *empdag;

   bool isTree;
   unsigned n_nonleaf;
   unsigned timestamp;
   unsigned num_visited;
   unsigned num_mps;
   unsigned num_mpes;
   unsigned num_nodes;
   unsigned max_depth;
   DfsState * restrict nodes_stat;
   unsigned * restrict preorder;
   unsigned * restrict postorder;
   unsigned * restrict topo_order;
   unsigned * restrict topo_order_revidx;
   DagMpPpty * restrict mp_ppty;
   DagMpePpty * restrict mpe_ppty;
   bool * restrict processed_vi;
   MpIdArray adversarial_mps;
   MpIdArray saddle_path_starts;
   unsigned processed_vi_len;
   bool hasVFPath;
} EmpDagDfsData;

typedef struct {
   unsigned ctrl_edges;
   unsigned len;
   unsigned *uids;
} TreePath;


typedef struct {
   TreePath path;
} AnalysisData;









NONNULL static
int runtime_error_state(DfsState state)
{
   error("[empdag] unexpected node state %u\n", state);
   return Error_EMPRuntimeError;
}

NONNULL static
int runtime_error_rc(int rc)
{
   error("[empdag] unexpected return code %d\n", rc);
   return Error_EMPRuntimeError;
}

NONNULL static
int runtime_error_type(MpType type)
{
   error("[empdag] unexpected MP type %s\n", mptype_str(type));
   return Error_EMPRuntimeError;
}

NONNULL static
int error_rc(int rc)
{
   assert(rc != 0);
   if (rc > 0) return rc;
   if (rc > -DagErrLen) {
      error("[empdag] Error message is '%s'\n", empdag_errstr(rc));
      return Error_EMPIncorrectInput;
   }

   error("[empdag] unexpected return code %d\n", rc);
   return Error_EMPRuntimeError;
}


static const char *nidx_badidxstr(void)
{
   return "ERROR node index is too big!";
}


NONNULL static inline
const char* nidx_typename(nidx_t nidx, const EmpDagDfsData *dfsdata)
{
   if (nidx >= dfsdata->num_nodes) { return nidx_badidxstr(); }
   return nidx < dfsdata->num_mps ? "MP" : "MPE";
}

NONNULL static inline
const char* nidx_getname(nidx_t nidx, const EmpDagDfsData *dfsdata)
{
   if (nidx >= dfsdata->num_nodes) { return nidx_badidxstr(); }
   return nidx < dfsdata->num_mps ? empdag_getmpname(dfsdata->empdag, nidx) :
      empdag_getmpename(dfsdata->empdag, nidx-dfsdata->num_mps);
}

NONNULL static inline
daguid_t nidx2uid(nidx_t nidx, const EmpDagDfsData *dfsdata)
{
   if (nidx >= dfsdata->num_nodes) { return EMPDAG_UID_NONE; }
   return nidx < dfsdata->num_mps ? mpid2uid(nidx) : mpeid2uid(nidx-dfsdata->num_mps);

}

NONNULL static inline
DfsState process_node_state(EmpDagDfsData *dfsdat, daguid_t uid, nidx_t node_idx)
{

   DfsState state = dfsdat->nodes_stat[node_idx];
   switch (state) {
   case NotExplored:
      dfsdat->nodes_stat[node_idx] = InProgress;
      break;
   case InProgress: {
      error("[empdag/analysis] Cycle detected! It involves the problem %s(%s)\n",
            daguid_type2str(uid), empdag_getname(dfsdat->empdag, uid));
      dfsdat->nodes_stat[node_idx] = CycleStart;
      return CycleStart;
   }
   case Processed: {
      dfsdat->isTree = false;
      return Processed;
   }
   default:
      error("[empdag/analysis] ERROR (unknown) It involves the problem %s(%s)\n",
            daguid_type2str(uid), empdag_getname(dfsdat->empdag, uid));
      dfsdat->nodes_stat[node_idx] = ErrorState;
      return runtime_error_state(state);
   }

   return dfsdat->nodes_stat[node_idx];
}

NONNULL static inline
void mark_as_processed(EmpDagDfsData *dfsdata, nidx_t node_idx)
{
   dfsdata->nodes_stat[node_idx] = Processed;
}

NONNULL static inline
DfsState get_state(EmpDagDfsData *dfsdata, nidx_t node_idx)
{
   return dfsdata->nodes_stat[node_idx];
}

NONNULL static inline
nidx_t nidx_parent(const EmpDagDfsData *dfsdata, nidx_t node_idx)
{
   UIntArray *parents;
   if (node_idx < dfsdata->num_mps) {
      parents = &dfsdata->empdag->mps.rarcs[node_idx];
   } else {
      parents = &dfsdata->empdag->mpes.rarcs[node_idx-dfsdata->num_mps];
   }
   unsigned num_parents = parents->len;

   if (num_parents == 1) {
      daguid_t uid_parent = parents->arr[0];
      unsigned id = uid2id(uid_parent);
      return uidisMP(uid_parent) ? id : id + dfsdata->num_mps;
   }

   if (num_parents == 0) {
      return NodeIdxNoParent;
   }
   if (num_parents > 1) {
      return NodeIdxDAG;
   }

   return NodeIdxError;
}

NONNULL static inline
int analysis_data_init(AnalysisData *analysis_data, EmpDagDfsData *dfsdata)
{
   MALLOC_(analysis_data->path.uids, unsigned, dfsdata->max_depth+1);

   /* Just to make sure those are initialized ... */
   analysis_data->path.len = UINT_MAX;
   analysis_data->path.ctrl_edges = UINT_MAX;

   return OK;
}

NONNULL static inline
void analysis_data_free(AnalysisData *analysis_data)
{
   FREE(analysis_data->path.uids);
}

NONNULL static 
void report_error_futurevar(const EmpDag *empdag, rhp_idx vi, rhp_idx ei,
                            mpid_t mp_vi, mpid_t mp_id)
{
   error("[empdag] ERROR: in equation '%s', the external variable '%s' belongs to MP(%s),"
         " a descendant of MP(%s). However, these two are linked by VF edges, "
         "which is not correct.\n", ctr_printequname(&empdag->mdl->ctr, ei),
         ctr_printvarname(&empdag->mdl->ctr, vi), empdag_getname(empdag, mp_vi),
         empdag_getname(empdag, mp_id));
}

NONNULL static 
void report_error_nolca(const EmpDag *empdag, rhp_idx vi, rhp_idx ei,
                      mpid_t mp_vi, mpid_t mp_id)
{
   error("[empdag] ERROR: in equation '%s', which belongs to MP(%s), "
         "the external variable '%s' belongs to MP(%s). No common ancestor between"
         " the two MP could be found\n", ctr_printequname(&empdag->mdl->ctr, ei),
         empdag_getname(empdag, mp_id), ctr_printvarname(&empdag->mdl->ctr, vi),
         empdag_getname(empdag, mp_vi));
}

NONNULL static 
void report_error_badlca(const EmpDag *empdag, rhp_idx vi, rhp_idx ei,
                      mpid_t mp_vi, mpid_t mp_id, daguid_t uid_lca)
{
   error("[empdag] ERROR: in equation '%s', which belongs to MP(%s), "
         "the external variable '%s' belongs to MP(%s). The common ancestor %s(%s) "
         " between the two MP is not an MPE, as it should be.\n",
         ctr_printequname(&empdag->mdl->ctr, ei),
         empdag_getname(empdag, mp_id), ctr_printvarname(&empdag->mdl->ctr, vi),
         empdag_getname(empdag, mp_vi), daguid_type2str(uid_lca),
         empdag_getname(empdag, uid_lca));
}

NONNULL static
int dfsdata_init(EmpDagDfsData *dfsdata, EmpDag * restrict empdag)
{
   dfsdata->mdl = empdag->mdl;
   dfsdata->empdag = empdag;

   dfsdata->isTree = true;
   dfsdata->hasVFPath = false;

   dfsdata->n_nonleaf = 0;
   dfsdata->timestamp = 0;
   dfsdata->num_visited = 0;
   dfsdata->num_mps = empdag->mps.len;
   dfsdata->num_mpes = empdag->mpes.len;
   dfsdata->num_nodes = empdag->mps.len + empdag->mpes.len;
   dfsdata->max_depth = 0;

   unsigned num_nodes = dfsdata->num_nodes;
   unsigned num_mps = dfsdata->num_mps;
   unsigned num_mpes = dfsdata->num_mpes;

   CALLOC_(dfsdata->nodes_stat, DfsState, num_nodes);
   CALLOC_(dfsdata->preorder, unsigned, num_nodes);
   CALLOC_(dfsdata->postorder, unsigned, num_nodes);
   CALLOC_(dfsdata->topo_order, unsigned, num_nodes);
   CALLOC_(dfsdata->topo_order_revidx, unsigned, num_nodes);
   MALLOC_(dfsdata->mp_ppty, DagMpPpty, num_mps);
   MALLOC_(dfsdata->mpe_ppty, DagMpePpty, num_mpes);

   dfsdata->processed_vi_len = ctr_nvars_total(&empdag->mdl->ctr);
   MALLOC_(dfsdata->processed_vi, bool, dfsdata->processed_vi_len);

   rhp_uint_init(&dfsdata->adversarial_mps);
   rhp_uint_init(&dfsdata->saddle_path_starts);



   return OK;
}

NONNULL static
void dfsdata_free(EmpDagDfsData *dfsdata)
{
   FREE(dfsdata->nodes_stat);
   FREE(dfsdata->preorder);
   FREE(dfsdata->postorder);
   FREE(dfsdata->topo_order);
   FREE(dfsdata->topo_order_revidx);
   FREE(dfsdata->mp_ppty);
   FREE(dfsdata->mpe_ppty);
   FREE(dfsdata->processed_vi);

   rhp_uint_empty(&dfsdata->adversarial_mps);
   /* DO not empty dfsdata->saddle_path_starts, it is passed to the empdag */
}

/* To extend this to a DAG, have a look at:
 * - https://www.youtube.com/watch?v=VIIzCLQ439c?t=68
 * - https://web.archive.org/web/20190914030055/http://www.gghh.name:80/dibtp/2014/02/25/how-does-mercurial-select-the-greatest-common-ancestor.html
 * - https://stackoverflow.com/questions/14865081/algorithm-to-find-lowest-common-ancestor-in-directed-acyclic-graph
 * - https://github.com/networkx/networkx/blob/main/networkx/algorithms/lowest_common_ancestors.py (BSD code)
 * - https://uhra.herts.ac.uk/handle/2299/16553 (restrictive license)
 * - https://doi.org/10.1016/j.ipl.2010.02.014 
 *   */

/* Further ressources on LCA:
 * - https://www.baeldung.com/cs/tree-lowest-common-ancestor
 */

NONNULL static
nidx_t lca(nidx_t u, nidx_t v, const EmpDagDfsData * dfsdata) {
   /* ---------------------------------------------------------------------
    * The gist of the algorithm is as follow:
    * - The [ preorder, postorder ] guarantues that any successor node has
    *   an interval which is a subset of its parent node.
    * - GOAL: find the node which interval is a superset of both original nodes
    * - The first two checks look for this.
    * - If they fail, then we move up the ancestor of the node with the largest
    *   preorder
    *
    * This is close to the "binary lifting", running in O(log N), see
    * https://cp-algorithms.com/graph/lca_binary_lifting.html#implementation
    * --------------------------------------------------------------------- */


   if (dfsdata->preorder[u] < dfsdata->preorder[v]) {

      if (dfsdata->postorder[u] > dfsdata->postorder[v]) { return u; }

      nidx_t parent_v = nidx_parent(dfsdata, v);
      return valid_nidx(parent_v) ? lca(u, parent_v, dfsdata) : parent_v;
   }

   /* we have dfsdata->preorder[v] <= dfsdata->preorder[u] */

   /* Here we test for >= as otherwise we might have issue if u and v are the
    * same node */
   if(dfsdata->postorder[v] >= dfsdata->postorder[u]) { return v; }

   nidx_t parent_u = nidx_parent(dfsdata, u);
   return valid_nidx(parent_u) ? lca(u, parent_u, dfsdata) : parent_u;
}

typedef enum {
   DfsPathUnset,
   DfsPathVFMin,
   DfsPathVFMax,
   DfsPathCtrl,
   DfsPathEquil,
   DfsPathLen,
} DagPathType;

typedef struct {
   bool        has_ctrl_edges;
   bool        saddle_path_registered;
   DagPathType pathtype;
   uint32_t    depth;
   mpid_t      saddle_path_start;
} DfsPathDataFwd;

static int dfs_mpC(mpid_t mpid_parent, EmpDagDfsData *dfsdata, DfsPathDataFwd pathdata);
static int dfs_mpV(mpid_t mpid_parent, EmpDagDfsData *dfsdata, DfsPathDataFwd pathdata);
static int dfs_mpe(mpeid_t id_parent, EmpDagDfsData *dfsdata, DfsPathDataFwd pathdata);

DBGUSED static inline 
bool pathtype_is_VF(DagPathType pathtype) {

  return pathtype == DfsPathVFMin || pathtype == DfsPathVFMax;
}

NONNULL static inline
DagPathType sense2pathtype(RhpSense sense)
{
   /* ---------------------------------------------------------------------
    * Generate the path type from the UID and MP type
    * --------------------------------------------------------------------- */

//   if (uidisMPE(uid_parent)) return DfsPathEquil;

   switch (sense) {
   case RhpMin:
      return DfsPathVFMin;
   case RhpMax:
      return DfsPathVFMax;
   default:
      return DfsPathUnset;
   }
}

NONNULL static int process_Carcs(EmpDagDfsData *dfsdata, UIntArray *Carcs,
                                 DfsPathDataFwd pathdata_child, unsigned nidx)
{
   int rc = 0;
   for (unsigned i = 0, len = Carcs->len; i < len; ++i) {
      daguid_t uid_child = Carcs->arr[i];

      pathdata_child.pathtype = DfsPathCtrl;
      pathdata_child.saddle_path_start = UINT_MAX;

      if (uidisMP(uid_child)) {
         rc = dfs_mpC(uid2id(uid_child), dfsdata, pathdata_child);
      } else {
         rc = dfs_mpe(uid2id(uid_child), dfsdata, pathdata_child);
      }
      if (rc == 0) continue;
      if (rc > 0) { return rc; }
      if (rc == -DagErrDagCycle) {
         error("MP(%s)\n", empdag_getname(dfsdata->empdag, uid_child));
         DfsState stat = get_state(dfsdata, nidx);
         if (stat == CycleStart) { return -DagErrGeneric; }
         return rc;
      }

   }

   /* Catch all error */
   if (rc == 0) return OK;

   return runtime_error_rc(rc);
}

NONNULL static int process_Varcs(EmpDagDfsData *dfsdata, struct VFedges *Varcs,
                                 DfsPathDataFwd pathdata, mpid_t mpid_parent)
{
   EmpDag *empdag = dfsdata->empdag;
   DagPathType cur_pathtype = pathdata.pathtype;

   dfsdata->hasVFPath = true;

   assert(Varcs->len > 0);

   for (unsigned i = 0, len = Varcs->len; i < len; ++i) {
      const ArcVFData *arcVF = &Varcs->arr[i];

      /* ------------------------------------------------------------------
       * If we have an adversarial MP, we add it to a list of MP to reformulate
       * This is the case when the edge is of VF type and the sense of the
       * path and the child MP are opposite.
       * ------------------------------------------------------------------ */

      mpid_t mpid_child = arcVF->child_id;
      const MathPrgm *mp_child = empdag->mps.arr[mpid_child];
      RhpSense sense = mp_getsense(mp_child);

      if ((sense == RhpMax && cur_pathtype == DfsPathVFMin) ||
          (sense == RhpMin && cur_pathtype == DfsPathVFMax)) {

         S_CHECK(mpidarray_add(&dfsdata->adversarial_mps, mpid_child));

         if (!pathdata.saddle_path_registered) {
            S_CHECK(mpidarray_add(&dfsdata->saddle_path_starts, pathdata.saddle_path_start));
            pathdata.saddle_path_registered = true;
         }
      } else if (sense == RhpFeasibility) {
         error("[empdag] ERROR: MP(%s), of type %s, is linked via a VF arc to "
               "its parent MP(%s). This is nonsensical.\n",
               empdag_getmpname(empdag, mpid_child), mp_gettypestr(mp_child),
               empdag_getmpname(empdag, mpid_parent));
         return Error_EMPIncorrectInput;
      } else if (sense != RhpMax && sense != RhpMin) {
         error("[empdag] ERROR: MP(%s) has unknown/unsupported sense %s\n",
               empdag_getmpname(empdag, mpid_child), sense2str(sense));
         return Error_EMPRuntimeError;
      }

      int rc = dfs_mpV(mpid_child, dfsdata, pathdata);

      if (rc == 0) continue;
      if (rc > 0) { return rc; }
      if (rc == -DagErrDagCycle) {
         error("MP(%s)\n", empdag_getmpname(empdag, mpid_child));
         DfsState stat = get_state(dfsdata, mpid_child);
         if (stat == CycleStart) { return -DagErrGeneric; }
         return rc;
      }
   }

   return OK;

}

NONNULL static
int dfs_mpC(mpid_t mpid, EmpDagDfsData *dfsdata, DfsPathDataFwd pathdata)
{
   const EmpDag * restrict empdag = dfsdata->empdag;
   unsigned node_idx = mpid; assert(mpid < empdag->mps.len);
   UIntArray *Carcs = &empdag->mps.Carcs[mpid];
   struct VFedges *Varcs = &empdag->mps.Varcs[mpid];

   assert(pathdata.pathtype == DfsPathCtrl);
   DfsPathDataFwd pathdata_child = pathdata;
   pathdata_child.depth++;


   DfsState state = process_node_state(dfsdata, mpid2uid(mpid), node_idx);
   switch (state) {
   case InProgress:
      break;
   case CycleStart:
      return -DagErrDagCycle;
   case Processed:
      return OK;
   default:
      return runtime_error_state(state);
   }

   dfsdata->preorder[node_idx] = ++dfsdata->timestamp;

   /* We are at a leaf node */
   unsigned Vlen = Varcs->len;
   if (Carcs->len == 0 && Vlen == 0) {
      dfsdata->max_depth = MAX(dfsdata->max_depth, pathdata.depth);
   }

   /* ---------------------------------------------------------------------
    * Children on CTRL edge do not require further analysis, just visit them
    * --------------------------------------------------------------------- */

   S_CHECK(process_Carcs(dfsdata, Carcs, pathdata_child, node_idx));

   /* ---------------------------------------------------------------------
    * Children on VF arc: we need to determine whether we have a saddle path
    * --------------------------------------------------------------------- */

   assert(!pathtype_is_VF(pathdata.pathtype));

   if (Vlen > 0) {
      RhpSense sense_parent = mp_getsense(empdag->mps.arr[mpid]);
      assert(sense_parent == RhpMax || sense_parent == RhpMin);
      DagPathType cur_pathtype = sense2pathtype(sense_parent);

      pathdata_child.pathtype = cur_pathtype;
      pathdata_child.saddle_path_start = mpid;
      pathdata_child.saddle_path_registered = false;

      S_CHECK(process_Varcs(dfsdata, Varcs, pathdata_child, mpid));
   }

   /* ---------------------------------------------------------------------
    * Post order: set the topo order
    * --------------------------------------------------------------------- */

   dfsdata->topo_order_revidx[node_idx] = dfsdata->num_visited;
   dfsdata->topo_order[dfsdata->num_visited++] = node_idx;

   dfsdata->postorder[node_idx] = ++dfsdata->timestamp;

   /* Mark node as processed so that no further exploration is required */
   mark_as_processed(dfsdata, node_idx);

   return OK;
}

/* ---------------------------------------------------------------------
 * This function processes a node that was accessed via a VF arc
 * --------------------------------------------------------------------- */

NONNULL static
int dfs_mpV(mpid_t mpid, EmpDagDfsData *dfsdata, DfsPathDataFwd pathdata)
{
   const EmpDag * restrict empdag = dfsdata->empdag;
   unsigned node_idx = mpid;
   assert(mpid < empdag->mps.len);
   UIntArray *Carcs = &empdag->mps.Carcs[mpid];
   struct VFedges *Varcs = &empdag->mps.Varcs[mpid];

   assert(pathtype_is_VF(pathdata.pathtype));
   DfsPathDataFwd pathdata_child = pathdata;
   pathdata_child.depth++;

   DfsState state = process_node_state(dfsdata, mpid2uid(mpid), node_idx);
   switch (state) {
   case InProgress:
      break;
   case CycleStart:
      return -DagErrDagCycle;
   case Processed:
      return OK;
   default:
      return runtime_error_state(state);
   }

   dfsdata->preorder[node_idx] = ++dfsdata->timestamp;

   /* We are at a leaf node */
   unsigned Vlen = Varcs->len;
   unsigned Clen = Carcs->len;
   if (Clen == 0 && Vlen == 0) {
      dfsdata->max_depth = MAX(dfsdata->max_depth, pathdata.depth);
   }

   /* ---------------------------------------------------------------------
    * Children on VF arc: we need to determine whether we have a saddle path
    * --------------------------------------------------------------------- */

   if (Vlen > 0 ) {
      S_CHECK(process_Varcs(dfsdata, Varcs, pathdata_child, mpid));
   }

   /* ---------------------------------------------------------------------
    * Children on CTRL arc: reset pathtype
    * --------------------------------------------------------------------- */

   if (Clen > 0) {
      pathdata_child.pathtype = DfsPathCtrl;
      S_CHECK(process_Carcs(dfsdata, Carcs, pathdata_child, node_idx));
   }

   dfsdata->topo_order_revidx[node_idx] = dfsdata->num_visited;
   dfsdata->topo_order[dfsdata->num_visited++] = node_idx;

   dfsdata->postorder[node_idx] = ++dfsdata->timestamp;

   /* Mark node as processed so that no further exploration is required */
   mark_as_processed(dfsdata, node_idx);

   return OK;
}

NONNULL static
int dfs_mpInNashOrRoot(mpid_t mpid, EmpDagDfsData *dfsdata, DfsPathDataFwd pathdata)
{
   const EmpDag * restrict empdag = dfsdata->empdag;
   unsigned node_idx = mpid;
   assert(mpid < empdag->mps.len);
   UIntArray *Carcs = &empdag->mps.Carcs[mpid];
   struct VFedges *Varcs = &empdag->mps.Varcs[mpid];

   assert(pathdata.pathtype == DfsPathEquil || pathdata.pathtype == DfsPathUnset);
   DfsPathDataFwd pathdata_child = pathdata;
   pathdata_child.depth++;

   DfsState state = process_node_state(dfsdata, mpid2uid(mpid), node_idx);
   switch (state) {
   case InProgress:
      break;
   case CycleStart:
      return -DagErrDagCycle;
   case Processed:
      return OK;
   default:
      return runtime_error_state(state);
   }

   dfsdata->preorder[node_idx] = ++dfsdata->timestamp;

   /* Are we at a leaf node? */
   unsigned Vlen = Varcs->len;
   if (Carcs->len == 0 && Vlen == 0) {
      dfsdata->max_depth = MAX(dfsdata->max_depth, pathdata.depth);
   }

   /* ---------------------------------------------------------------------
    * Children on VF arc: we need to determine whether we have a saddle path
    * --------------------------------------------------------------------- */

   if (Vlen > 0) {
      RhpSense sense = mp_getsense(empdag->mps.arr[mpid]);
      assert(sense == RhpMax || sense == RhpMin);
      DagPathType cur_pathtype = sense2pathtype(sense);
      pathdata_child.pathtype = cur_pathtype;
      pathdata_child.saddle_path_start = mpid;
      pathdata_child.saddle_path_registered = false;

      S_CHECK(process_Varcs(dfsdata, Varcs, pathdata_child, mpid));
   }

   /* ---------------------------------------------------------------------
    * Children on CTRL arc: reset pathtype
    * --------------------------------------------------------------------- */

   pathdata_child.pathtype = DfsPathCtrl;
   S_CHECK(process_Carcs(dfsdata, Carcs, pathdata_child, node_idx));

   dfsdata->topo_order_revidx[node_idx] = dfsdata->num_visited;
   dfsdata->topo_order[dfsdata->num_visited++] = node_idx;

   dfsdata->postorder[node_idx] = ++dfsdata->timestamp;

   /* Mark node as processed so that no further exploration is required */
   mark_as_processed(dfsdata, node_idx);

   return OK;
}

NONNULL static
int dfs_mpe(mpeid_t id_parent, EmpDagDfsData *dfsdata, DfsPathDataFwd pathdata)
{
   const EmpDag * restrict empdag = dfsdata->empdag;
   unsigned node_idx = id_parent + dfsdata->num_mps;

   UIntArray *arcs = &empdag->mpes.arcs[id_parent];
   DfsPathDataFwd pathdata_child = pathdata;
   pathdata_child.depth++;

   DfsState state = process_node_state(dfsdata, mpeid2uid(id_parent), node_idx);
   switch (state) {
   case InProgress:
      break;
   case CycleStart:
      return -DagErrDagCycle;
   case Processed:
      return OK;
   default:
      return runtime_error_state(state);
   }

   dfsdata->preorder[node_idx] = ++dfsdata->timestamp;

   /* We are at a leaf node */
   if (arcs->len == 0) {
      error("[empdag] ERROR: MPE(%s) has no child.\n", empdag_getmpename(empdag, id_parent));
      return -DagErrMpeNoChild;
   }


   pathdata_child.pathtype = DfsPathEquil;

   for (unsigned i = 0, len = arcs->len; i < len; ++i) {
      daguid_t uid_child = arcs->arr[i];
      assert(uidisMP(uid_child));

      int rc = dfs_mpInNashOrRoot(uid2id(uid_child), dfsdata, pathdata_child);

      if (rc == 0) continue;
      if (rc > 0) { return rc; }
      if (rc == -DagErrDagCycle) {
         error("MPE(%s)\n", empdag_getmpename(empdag, id_parent));
          DfsState stat = get_state(dfsdata, node_idx);
         if (stat == CycleStart) { return -DagErrGeneric; }
         return rc;
      }
   }

   dfsdata->topo_order_revidx[node_idx] = dfsdata->num_visited;
   dfsdata->topo_order[dfsdata->num_visited++] = node_idx;

   dfsdata->postorder[node_idx] = ++dfsdata->timestamp;

   /* Mark node as processed so that no further exploration is required */
   mark_as_processed(dfsdata, node_idx);

   return OK;
}

DBGUSED NONNULL static inline
bool empdag_chk_vitype(const VarMeta *varmeta)
{
   /* TODO(URG): this should not always return true */
   return true;
}

NONNULL static inline
bool is_ancestor(EmpDagDfsData *dfsdata, mpid_t mp_parent, mpid_t mp_child)
{
   assert(mp_parent < dfsdata->num_mps && mp_child < dfsdata->num_mps);

   /* For MP, nodeidx and mpid are the same */
   return (dfsdata->preorder[mp_parent] < dfsdata->preorder[mp_child]) &&
          (dfsdata->postorder[mp_parent] > dfsdata->postorder[mp_child]);
}


NONNULL static inline
int tree_get_path_backward(EmpDagDfsData *dfsdata, mpid_t mp_parent, mpid_t mp_child,
                           TreePath *path)
{
   assert(mp_child != mp_parent);
   assert(mp_parent < dfsdata->num_mps && mp_child < dfsdata->num_mps);

   const DagMpArray *mps = &dfsdata->empdag->mps;
   const DagMpeArray *mpes = &dfsdata->empdag->mpes;

   /* ---------------------------------------------------------------------
    * To get the path between 2 nodes, the easiest is to go up from the
    * "child" node towards its ancestor.
    * --------------------------------------------------------------------- */

   path->ctrl_edges = 0;
   path->len = 0;

   daguid_t uid = mpid2uid(mp_child);

   while (!uidisMP(uid) || uid2id(uid) != mp_parent) {
      assert(path->len <= dfsdata->max_depth);

      UIntArray *rarcs;
      if (uidisMP(uid)) {
         rarcs = &mps->rarcs[uid2id(uid)];
      } else {
         assert(uidisMPE(uid));
         rarcs = &mpes->rarcs[uid2id(uid)];
      }

      uid = rarcs->len > 0 ? rarcs->arr[0] : EMPDAG_UID_NONE;

      if (!valid_uid(uid)) {
         error("[empdag] Invalid uid when going from MP(%s) to MP(%s)\n",
               empdag_getmpname(dfsdata->empdag, mp_parent),
               empdag_getmpname2(dfsdata->empdag, mp_child));
         return Error_EMPRuntimeError;
      }
      
      if (uidisMP(uid) && rarcTypeCtrl(uid)) { path->ctrl_edges++; }
      path->uids[path->len++] = uid;
   }

   return OK;
}

NONNULL static inline
bool is_parent(const UIntArray *rarcs, mpid_t mp_id, daguid_t *uid)
{
   if (rarcs->len == 0) { return false; }
   assert(rarcs->len == 1);

   daguid_t uid_ = rarcs->arr[0];

   if (uid2id(uid_) != mp_id) { return false; };

   *uid = uid_;

   return true;
}

NONNULL static inline
bool is_child_Carcs(const UIntArray *arcs, mpid_t mp_id)
{
   for (unsigned i = 0, len = arcs->len; i < len; ++i) {
      if (uid2id(arcs->arr[i]) == mp_id) return true;
   }

   return false;
}

NONNULL static inline
bool is_child_Varcs(const struct VFedges *Varcs, mpid_t mp_id)
{
   for (unsigned i = 0, len = Varcs->len; i < len; ++i) {
      if (Varcs->arr[i].child_id == mp_id) return true;
   }

   return false;
}

NONNULL static
int mp_opt_ctrledge(EmpDag *empdag, unsigned level, MpType type)
{
   switch (type) {
   case MpTypeOpt:
      empdag_opt_consfeature(empdag, EmpDag_OptSolMapConstraint);
      break;
   case MpTypeVi:
/* See GITLAB #83 */
//   case RHP_MP_MCP:
      empdag_opt_consfeature(empdag, EmpDag_ViConstraint);
      break;
   default:
      return runtime_error_type(type);
   }

   if (level > 1) {
      empdag_vi_consfeature(empdag, EmpDag_Vi_MultiLevelConstraint);
   }

   return OK;
}

NONNULL static
int mp_vi_ctrledge(EmpDag *empdag, unsigned level, MpType type)
{
   switch (type) {
   case MpTypeOpt:
      empdag_vi_consfeature(empdag, EmpDag_Vi_OptSolMapConstraint);
      break;
   case MpTypeVi:
/* See GITLAB #83 */
//   case RHP_MP_MCP:
      empdag_vi_consfeature(empdag, EmpDag_Vi_ViConstraint);
      break;
   default:
      return runtime_error_type(type);
   }

   if (level > 1) {
      empdag_vi_consfeature(empdag, EmpDag_Vi_MultiLevelConstraint);
   }

   return OK;
}

NONNULL static inline
int mp_ctrledge(EmpDag *empdag, unsigned lvl, MpType parent_type, MpType child_type)
{
   switch (parent_type) {
   case MpTypeOpt:
      return mp_opt_ctrledge(empdag, lvl, child_type);
   case MpTypeVi:
/* See GITLAB #83 */
//   case RHP_MP_MCP:
      return mp_vi_ctrledge(empdag, lvl, child_type);
   default:
      return runtime_error_type(child_type);
   }

}

NONNULL static
bool valid_foreign_equ_ctrlchildren(const Model *mdl, rhp_idx ei, mpid_t mp_id,
                                   EmpDagDfsData *dfsdata)
{
   bool mdl_is_rhp_ = mdl_is_rhp(mdl);

   VarMeta *varmeta = mdl->ctr.varmeta;
   void *iterator = NULL;
   double jacval;
   rhp_idx vi;
   int nlflags;

   do {
      if (mdl_is_rhp_) {
         S_CHECK(rctr_walkequ(&mdl->ctr, ei, &iterator, &jacval, &vi, &nlflags));
      } else {
         assert(mdl->backend == RHP_BACKEND_GAMS_GMO);
         S_CHECK(ctr_equ_itervars(&mdl->ctr, ei, &iterator, &jacval, &vi, &nlflags));
      }
   
  
      assert(empdag_chk_vitype(&varmeta[vi]));
      mpid_t mp_var = varmeta[vi].mp_id;
   
      /* TODO: better error reporting */
      if (!valid_mpid(mp_var)) { return false; }

      /* ----------------------------------------------------------------
       * We only complain if the equation involves variables from ancestor
       * nodes. If we find a variable that does not fit this description,
       * bail early.
       * ---------------------------------------------------------------- */

       if (!is_ancestor(dfsdata, mp_var, mp_id)) {
         return true;
      }


   } while (iterator);

   return false;
}

NONNULL static
int report_error_foreign_equ(const Model *mdl, rhp_idx ei, mpid_t mp_id)
{
   const EmpDag *empdag = &mdl->empinfo.empdag;
   bool mdl_is_rhp_ = mdl_is_rhp(mdl);
   UIntArray mps;
   rhp_uint_init(&mps);
   int pos;

   error("[empdag] %nERROR in MP(%s): the equation '%s' does not invove "
         "any variable attached to this MP, only external ones.\n" , &pos,
         empdag_getmpname(empdag, mp_id),
         mdl_printequname(mdl, ei));
   error("%*sThis is known to produce a model with inferior solvability "
         "guarantees.\n", pos, "");
   error("%*sThe equation '%s' involves the following variables:\n",
         pos, "", mdl_printequname(mdl, ei));

   VarMeta *varmeta = mdl->ctr.varmeta;
   void *iterator = NULL;
   double jacval;
   rhp_idx vi;
   int nlflags;

   do {
      if (mdl_is_rhp_) {
         S_CHECK(rctr_walkequ(&mdl->ctr, ei, &iterator, &jacval, &vi, &nlflags));
      } else {
         assert(mdl->backend == RHP_BACKEND_GAMS_GMO);
         S_CHECK(ctr_equ_itervars(&mdl->ctr, ei, &iterator, &jacval, &vi, &nlflags));
      }
   
      /* ----------------------------------------------------------------
            * Get the MP owning this variable. We do this to tag the equation
            * as being not completely exogenous. We could optimize and only do
            * that if the equation has not being tagged as containing variables
            * ---------------------------------------------------------------- */
   
      assert(empdag_chk_vitype(&varmeta[vi]));
      mpid_t mp_var = varmeta[vi].mp_id;
   
      /* This kind of error will be reported on their own */
      if (!valid_mpid(mp_var)) { continue; }

      error("%*sVAR %-30s belongs to MP(%s)\n", pos, "", mdl_printvarname(mdl, vi),
            empdag_getmpname(empdag, mp_var));
      
      if (rhp_uint_findsorted(&mps, mp_var) == UINT_MAX) {
         S_CHECK(rhp_uint_addsorted(&mps, mp_var))
      }
   } while (iterator);

   if (mps.len == 1) {
      error("%*sSuggestion: assign the equation to the MP(%s) to fix this error\n",
            pos, "", empdag_getmpname(empdag, mps.arr[0]));
   } else {
      error("%*sAssign this equation to another MP.\n", pos, "");
   }
   return OK;
}

NONNULL static
int analyze_mp(EmpDagDfsData *dfsdata, mpid_t mp_id, AnalysisData *data)
{
   const DagMpArray *mps = &dfsdata->empdag->mps;
   const MathPrgm * restrict mp = mps->arr[mp_id]; assert(mp);
   DagMpPpty * restrict mp_ppty = &dfsdata->mp_ppty[mp_id];
   const IdxArray * restrict equs = &mp->equs;
   const Model *mdl = dfsdata->empdag->mdl;
   VarMeta *varmeta = mdl->ctr.varmeta;

   bool *processed_vi = dfsdata->processed_vi;
   memset(processed_vi, 0, sizeof(bool)*dfsdata->processed_vi_len);

   /* ---------------------------------------------------------------------
    * Look for the level of parents and compute a lower bound on the current one
    * --------------------------------------------------------------------- */

   unsigned level;
   MpType type = mp_gettype(mp);

   if (mps->rarcs[mp_id].len > 0) {
      daguid_t uid = mps->rarcs[mp_id].arr[0];
      dagid_t pid = uid2id(uid);

      bool parent_is_MP = uidisMP(uid);
      level = parent_is_MP ? dfsdata->mp_ppty[pid].level : dfsdata->mpe_ppty[pid].level;
      /* If the parent is an equilibrium, the */
      if (parent_is_MP && rarcTypeCtrl(uid)) {
         level += 1;
         MpType parent_type = mp_gettype(mps->arr[pid]);
         S_CHECK(mp_ctrledge(dfsdata->empdag, level, parent_type, type));
      }

   } else {
      level = 0;
   }

   for (unsigned i = 1, len = mps->rarcs[mp_id].len; i < len; ++i) {
      daguid_t uid = mps->rarcs[mp_id].arr[i];
      dagid_t pid = uid2id(uid);
      unsigned l = dfsdata->mp_ppty[pid].level;

      if (uidisMP(uid) && rarcTypeCtrl(uid)) {
         l += 1;
         MpType parent_type = mp_gettype(mps->arr[pid]);
         S_CHECK(mp_ctrledge(dfsdata->empdag, level, parent_type, type));
      }

      if (l != level) {
         error("[empdag] MP(%s) has different levels by different parents: %u "
               "vs %u\n", mps->names[mp_id], l, level);
         return Error_NotImplemented;
      }
   }

   mp_ppty->level = level;

   if (level > 1) {
      switch (type) {
      case MpTypeOpt:
      case MpTypeCcflib:
         empdag_opt_consfeature(dfsdata->empdag, EmpDag_MultiLevelConstraint);
         break;
      case MpTypeVi:
/* See GITLAB #83 */
//      case RHP_MP_MCP:
         empdag_vi_consfeature(dfsdata->empdag, EmpDag_Vi_MultiLevelConstraint);
         break;
      default: return runtime_error_type(type);
      }
   }

   unsigned num_err = 0;
   bool mdl_is_rhp_ = mdl_is_rhp(mdl);
   UNUSED const Fops * restrict fops = mdl_is_rhp_ ? rmdl_getfops(mdl) : NULL;

   for (unsigned i = 0, len = equs->len; i < len; ++i) {
      rhp_idx ei = equs->arr[i];
      assert(valid_ei(ei));
      bool equ_has_owned_var = false, equ_is_cst = false;

      assert(!fops || fops->keep_equ(fops->data, ei));

      void *iterator = NULL;
      double jacval;
      rhp_idx vi;
      int nlflags;
      daguid_t uid;
      do {
         if (mdl_is_rhp_) {
            S_CHECK(rctr_walkequ(&mdl->ctr, ei, &iterator, &jacval, &vi, &nlflags));
         } else {
            assert(mdl->backend == RHP_BACKEND_GAMS_GMO);
            S_CHECK(ctr_equ_itervars(&mdl->ctr, ei, &iterator, &jacval, &vi, &nlflags));
         }

         /* when the equation is a constant, vi is invalid on first call,
          * so don't treat this as an error */
         if (!valid_vi(vi)) {
            equ_is_cst = true;
            assert(!iterator);
            break;
         }

         /* ----------------------------------------------------------------
         * Get the MP owning this variable. We do this to tag the equation
         * as being not completely exogenous. We could optimize and only do
         * that if the equation has not being tagged as containing variables
         * ---------------------------------------------------------------- */

         assert(empdag_chk_vitype(&varmeta[vi]));
         mpid_t mp_var = varmeta[vi].mp_id;

         if (!valid_mpid(mp_var)) {
            int offset;
            error("[empdag] ERROR: var %n'%s' is not attached to any MP!\n",
                  &offset, mdl_printvarname(mdl, vi));
            error("%*sIt appears in equ '%s' of MP(%s)\n", offset, "",
                  mdl_printequname(mdl, ei), empdag_getmpname(dfsdata->empdag, mp_id));
            num_err++;
            continue;
         }

         /* Check whether the equation contains variables from the MP */
         if (mp_var == mp_id) { equ_has_owned_var = true; }


         if (processed_vi[vi]) { continue; }
         processed_vi[vi] = true;

         /* We are OK with this case */
         if (!mpid_regularmp(mp_var)) { continue; }

         assert(mp_var < mps->len);

         /* Simplest case: the MP owns the variable */
         if (mp_var == mp_id) {
            mp_ppty->num_ownvar++;
            continue;
         }

         /* A (direct) child MP owns the variable: CTRL cases */
         if (is_child_Carcs(&mps->Carcs[mp_id], mp_var)) {

            /* objvar do not get tagged as not really part of the problem */
            if (var_is_defined_objvar(&varmeta[vi])) { continue; }

            mp_ppty->num_solvar++;
            varmeta[vi].ppty |= VarIsSolutionVar;
            continue;
         }

         /* A variable from a descendent VF MP cannot be present in the equation */
         if (is_child_Varcs(&mps->Varcs[mp_id], mp_var)) {
            report_error_futurevar(dfsdata->empdag, vi, ei, mp_var, mp_id);
            continue;
         }

         /* The parent MP owns the variable: VF and CTRL subcases */
         if (is_parent(&mps->rarcs[mp_id], mp_var, &uid)) {

            if (rarcTypeVF(uid)) {
               mp_ppty->num_history++;
            } else {
               assert(rarcTypeCtrl(uid));
               mp_ppty->num_ctrlvar++;
            }

            continue;
         }

         /* Now we look for a ancestor-descendant relationship */
         TreePath *path = &data->path;

         /* Check for a case of history/CTRL variable */
         if (is_ancestor(dfsdata, mp_var, mp_id)) {
            S_CHECK(tree_get_path_backward(dfsdata, mp_var, mp_id, path));

            /* If the path contains a CTRL edge, the variable is a CTRL one.
             * Otherwise it is a history one */
            if (path->ctrl_edges > 0) {
               mp_ppty->num_ctrlvar++;
               assert(level == dfsdata->mp_ppty[mp_var].level + path->ctrl_edges);
            } else {
               mp_ppty->num_history++;
            }

            continue;
         }

         /* Check for solvar */
         if (is_ancestor(dfsdata, mp_id, mp_var)) {
            S_CHECK(tree_get_path_backward(dfsdata, mp_id, mp_var, path));

            /* We enforce that a CTRL edge must be present */
            if (path->ctrl_edges == 0) {
               report_error_futurevar(dfsdata->empdag, vi, ei, mp_var, mp_id);
               num_err++;
            } else {

               /* objvar do not get tagged as not really part of the problem */
               if (var_is_defined_objvar(&varmeta[vi])) { continue; }

               varmeta[vi].ppty |= VarIsSolutionVar;
               mp_ppty->num_solvar++;
               
            /* ---------------------------------------------------------------
             * We need to modify the empdag features here as the child node
             * might not depend on the parent node variables. This assymetry
             * is possible and not a mistake
             * TODO(xhub) document an example and check the code below
             * --------------------------------------------------------------- */

               unsigned child_lvl = path->ctrl_edges + level;

               if (level <= 1 && child_lvl > 1) {
                  const MathPrgm *mp_child = mps->arr[mp_var];
                  MpType child_type = mp_gettype(mp_child);
                  S_CHECK(mp_ctrledge(dfsdata->empdag, child_lvl, type, child_type));
               } else if (level == 0 && child_lvl == 1) {
                  const MathPrgm *mp_child = mps->arr[mp_var];
                  MpType child_type = mp_gettype(mp_child);
                  S_CHECK(mp_ctrledge(dfsdata->empdag, 1, type, child_type));
               }
            }

            continue;
         }


         /* -----------------------------------------------------------------
          * If we reached this point, the last option is an equilibrium variable
          * We proceed to look for the least common ancestor and check
          * that it is an MPE node
          * ---------------------------------------------------------------- */
         nidx_t nidx_lca = lca(mp_id, mp_var, dfsdata);
         if (!valid_uid(nidx_lca)) {
            report_error_nolca(dfsdata->empdag, vi, ei, mp_var, mp_id);
         }
         daguid_t nidx_uid = nidx2uid(nidx_lca, dfsdata);
         if (!uidisMPE(nidx_uid)) {
            report_error_badlca(dfsdata->empdag, vi, ei, mp_var, mp_id, nidx_uid);
            num_err++;
         } else {
             mp_ppty->num_nashvar++;
         }

      } while (iterator);

      if (!equ_is_cst && !equ_has_owned_var &&
         !valid_foreign_equ_ctrlchildren(mdl, ei, mp_id, dfsdata)) {
         report_error_foreign_equ(mdl, ei, mp_id);
         num_err++;
      }
   }

   if (num_err > 0) {
      error("[empdag] %u errors found while checking MP(%s)\n", num_err,
            empdag_getmpname(dfsdata->empdag, mp_id));
      return -num_err;
   }

   return OK;
}

NONNULL static
int analyze_mpe(EmpDagDfsData *dfsdata, mpid_t mpe_id, AnalysisData *data)
{
   EmpDag *empdag = dfsdata->empdag;
   const DagMpeArray *mpes = &empdag->mpes;
   DagMpePpty * restrict mpe_ppty = &dfsdata->mpe_ppty[mpe_id];
   unsigned num_err = 0;

   /* ---------------------------------------------------------------------
    * We just want to compute the level of the mpe node.
    * An MPE node can only have an MP as parent.
    * --------------------------------------------------------------------- */

   unsigned level;
   const DagMpArray *mps = &empdag->mps;
   bool mp_parent_opt = false, mp_parent_vi = false;

   if (mpes->rarcs[mpe_id].len > 0) {
      daguid_t uid = mpes->rarcs[mpe_id].arr[0];
      dagid_t pid = uid2id(uid);
      assert(uidisMP(pid));

      level = dfsdata->mp_ppty[pid].level;
      MpType type = mp_gettype(mps->arr[pid]);
      switch (type) {
      case MpTypeVi:  mp_parent_vi  = true; break;
      case MpTypeOpt: mp_parent_opt = true; break;
      default: return runtime_error_type(type);
      }

   } else {
      level = 0;
   }

   for (unsigned i = 1, len = mpes->rarcs[mpe_id].len; i < len; ++i) {
      daguid_t uid = mpes->rarcs[mpe_id].arr[i];
      dagid_t pid = uid2id(uid);
      assert(uidisMP(pid));
      unsigned l = dfsdata->mp_ppty[pid].level;

      if (l != level) {
         error("[empdag] MPE(%s) has different levels by different parents: %u "
               "vs %u\n", mpes->names[mpe_id], l, level);
         return Error_NotImplemented;
      }

      MpType type = mp_gettype(mps->arr[pid]);
      switch (type) {
      case MpTypeVi:  mp_parent_vi  = true; break;
      case MpTypeOpt: mp_parent_opt = true; break;
      default: return runtime_error_type(type);
      }
   }

   mpe_ppty->level = level;

   EmpDagViEdgeFeatures  vifeat;
   EmpDagOptEdgeFeatures optfeat;
   if (level > 1) {
      vifeat = EmpDag_Vi_MultiLevelConstraint;
      optfeat = EmpDag_MultiLevelConstraint;
   } else {
      vifeat = EmpDag_Vi_EquilConstraint;
      optfeat = EmpDag_EquilConstraint;
   }

   if (mp_parent_vi) {
      empdag_vi_consfeature(empdag, vifeat);
   }

   if (mp_parent_opt) {
      empdag_opt_consfeature(empdag, optfeat);
   }

   if (num_err > 0) {
      error("[empdag] %u errors found while checking MP(%s)\n", num_err,
            empdag_getmpename(dfsdata->empdag, mpe_id));
   }

   return OK;
}

int empdag_analysis(EmpDag * restrict empdag)
{
   int status = OK;
   double start = get_thrdtime();
   EmpDagDfsData dfsdata;
   S_CHECK(dfsdata_init(&dfsdata, empdag));

   /* For now we assume that there is only 1 root, could be multiple ones.
    * Lots has to be changed to extend this. This includes LCA */
   if (empdag->roots.len > 1) {
      error("[empdag] ERROR: EMPDAG of %s model '%.*s' #%u has %u roots. This "
            "is not yet supported\n", mdl_fmtargs(empdag->mdl), empdag->roots.len);
      return Error_EMPIncorrectInput;
   }

   if (empdag->roots.len == 0) {
      error("[empdag] ERROR: EMPDAG of %s model '%.*s' #%u has not root\n",
            mdl_fmtargs(empdag->mdl));
      return Error_EMPIncorrectInput;
   }

   daguid_t root_uid =  empdag->roots.arr[0];
   if (uidisMP(root_uid)) {
      mpid_t mpid = uid2id(root_uid);
      assert(mpid < empdag->mps.len);

      MpType mptype = mp_gettype(empdag->mps.arr[mpid]);
      switch (mptype) {
      case MpTypeOpt:
      case MpTypeCcflib:
         empdag_setrootprobtype(empdag, EmpDag_RootOpt);
         break;
      case MpTypeVi:
/* See GITLAB #83 */
//      case MpTypeMcp:
         empdag_setrootprobtype(empdag, EmpDag_RootVi);
         break;
      default:
         error("[empdag] ERROR: unsupported root MP with type %s",
               mptype_str(mptype));
         return Error_EMPRuntimeError;
      }
      
   } else {
      empdag_setrootprobtype(empdag, EmpDag_RootEquil);
   }

   for (unsigned i = 0, len = empdag->roots.len; i < len; ++i) {
      daguid_t uid = empdag->roots.arr[i], idx = uid2id(uid);

      DfsPathDataFwd pathdata = {.depth = 0, .has_ctrl_edges = false,
                                 .pathtype = DfsPathUnset,
                                 .saddle_path_start = UINT_MAX};

      int rc;
      if (uidisMP(uid)) {
         rc = dfs_mpInNashOrRoot(idx, &dfsdata, pathdata);
      } else {
         rc = dfs_mpe(idx, &dfsdata, pathdata);
      }

      if (rc != 0) return error_rc(rc);
   }

   /* ---------------------------------------------------------------------
    * If we did not visit all the nodes, report the ones that not connected
    * to the DAG
    * --------------------------------------------------------------------- */

   if (dfsdata.num_visited < dfsdata.num_nodes) {
      errormsg("[empdag:check] ERROR: some problems are not present in the graph:\n");
      unsigned missing_nodes = dfsdata.num_nodes - dfsdata.num_visited;
      unsigned num_visited = 0;

      do {
         missing_nodes--;

         while (dfsdata.nodes_stat[num_visited] == Processed) { num_visited++; }

         switch (dfsdata.nodes_stat[num_visited]) {
         case NotExplored: break;
         default:
            runtime_error_state(dfsdata.nodes_stat[num_visited]);
         }

         error("\t%s(%s)\n", nidx_typename(num_visited, &dfsdata),
               nidx_getname(num_visited, &dfsdata));
         num_visited++;

      } while (missing_nodes > 0);

      return Error_EMPIncorrectInput;
   }

   /* Now we can perform all the checks */
   AnalysisData analysis_data;
   unsigned num_issues = 0;
   unsigned num_mps = dfsdata.num_mps;
   S_CHECK(analysis_data_init(&analysis_data, &dfsdata));

   for (unsigned len = dfsdata.num_nodes, i = len-1; i < len; --i) {
      unsigned node_idx = dfsdata.topo_order[i];
      assert(node_idx < dfsdata.num_nodes);

      int rc;
      if (node_idx < num_mps)  { rc = analyze_mp(&dfsdata, node_idx, &analysis_data); }
      else { rc = analyze_mpe(&dfsdata, node_idx-num_mps, &analysis_data); }

      if (rc > 0) { status = rc; goto _exit_analysis_loop; }
      if (rc < 0) { num_issues++; }
   }

   if (num_issues > 0) {
      goto _exit_analysis_loop;
   }

   /* ---------------------------------------------------------------------
    * Printout some stats here
    * --------------------------------------------------------------------- */
   unsigned num_adversarial = dfsdata.adversarial_mps.len;
   if (O_Output & PO_TRACE_EMPDAG) {
      if (num_adversarial > 0) {
         trace_empdag("[empdag/analysis] There are %u adversarial MPs in %u "
                      "saddle paths.\n", num_adversarial,
                      dfsdata.saddle_path_starts.len);
      }
   }

   /* ---------------------------------------------------------------------
    * If there are adversarial MPS, sort them according to the topo order and
    * save the list to process in the EMPDAG.
    * --------------------------------------------------------------------- */

   if (num_adversarial > 0) {

      unsigned * restrict mps_id = dfsdata.adversarial_mps.arr;
      struct sort_obj * restrict mps2reformulate;
      CALLOC_(mps2reformulate, struct sort_obj, num_adversarial);

      for (unsigned i = 0; i < num_adversarial; ++i) {
         unsigned mp_id = mps_id[i];
         mps2reformulate[i].mp_id = mp_id;
         assert(dfsdata.topo_order_revidx[mp_id] <= INT_MAX);
         mps2reformulate[i].vi = (int)dfsdata.topo_order_revidx[mp_id];
      }

      empdag_sort_tim_sort(mps2reformulate, num_adversarial);

      mpidarray_reserve(&empdag->mps2reformulate, num_adversarial);
      unsigned * restrict mps2r = empdag->mps2reformulate.arr;


      for (unsigned i = 0; i < num_adversarial; ++i) {
         mps2r[i] = mps2reformulate[i].mp_id;
      }

      empdag->mps2reformulate.len = num_adversarial;

      /* Save the saddle_path_starts */
      memcpy(&empdag->saddle_path_starts, &dfsdata.saddle_path_starts, sizeof(UIntArray));

      FREE(mps2reformulate);
   }

   empdag->features.istree = dfsdata.isTree;
   empdag->features.hasVFpath = dfsdata.hasVFPath;

_exit_analysis_loop:
   analysis_data_free(&analysis_data);
   dfsdata_free(&dfsdata);

   if (num_issues > 0) {
      error("[empdag:check] analysis yielded %u nodes with issues\n", num_issues);
      status = status == OK ? Error_EMPIncorrectInput : status;
   }

   simple_timing_add(&empdag->mdl->timings->empdag.analysis, get_thrdtime() - start);

   return status;
}
