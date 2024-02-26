#ifndef EMPDAG_H
#define EMPDAG_H

#include <limits.h>

#include "empdag_common.h"
#include "empdag_uid.h"
#include "mdl_data.h"
#include "reshop_data.h"
#include "rhp_fwd.h"

/** @file empdag.h
 *
 * @brief EMP DAG structure
 *
 */

struct empdag_edge;

/** Edges for value function */
typedef struct VFedges {
  unsigned len;
  unsigned max;
  Varc *arr;
} VarcArray;

typedef struct mp_namedarray {
  unsigned len;
  unsigned max;
  const char **names;
  MathPrgm **arr;
  DagUidArray *Carcs;                 /**< Control edges                        */
  VarcArray *Varcs;                   /**< VF edges                             */
  DagUidArray *rarcs;                 /**< Parents (reverse arcs) indices       */
} DagMpArray;

typedef struct mpe_namedarray {
  unsigned len;
  unsigned max;
  const char **names;
  Mpe **arr;
  DagUidArray *arcs;       /**< Children indices                     */
  DagUidArray *rarcs;      /**< EMPDAG Parents (reverse arcs) indices       */
} DagMpeArray;

/** @brief Variational Equilibrium details */
struct vi_equil {
  bool compute_ve;   /**< Compute the variational equilibrium */
  bool full_ve;      /**< All shared constraints have the same multiplier */
  Aequ *common_equs; /**< List of common equations */
  Aequ *common_mult; /**< List of constraints with the same multiplier.
  This is only looked at if full_ve is false */
};

/** @brief Equilibrium structure */
typedef struct rhp_equilibrium {
  unsigned id;
  struct vi_equil ve;        /**< Variational Equilibrium details */
   Model *mdl;
  /* TODO: variational equilibrium with EPEC? */
} Mpe;

/** Types of EMPDAG */
typedef enum empdag_type {
  EmpDag_Unset,
  EmpDag_Empty,             /**< No EMPDAG given */
  EmpDag_Single_Opt,        /**< Single optimization agent */
  EmpDag_Single_Vi,         /**< Single VI agent           */
  EmpDag_Opt,
  EmpDag_Vi,
  EmpDag_Mopec,
  EmpDag_Bilevel,
  EmpDag_Multilevel,
  EmpDag_MultilevelMopec,  /**< */
  EmpDag_Mpec,
  EmpDag_Epec,
  EmpDag_NestedCcf,
  EmpDag_Complex,
} EmpDagType;

typedef enum {
   EmpDag_RootUnset        = 0,
   EmpDag_RootOpt          = 1,
   EmpDag_RootVi           = 2,
   EmpDag_RootEquil        = 3,
} EmpDagRootType;

typedef enum {
   EmpDag_SimpleConstraint      = 0,
   EmpDag_EquilConstraint       = 1,
   EmpDag_ViConstraint          = 2,
   EmpDag_OptSolMapConstraint   = 4,
   EmpDag_MultiLevelConstraint  = 8,
} EmpDagOptEdgeFeatures;

typedef enum {
   EmpDag_Vi_SimpleConstraint      = 0,
   EmpDag_Vi_EquilConstraint       = 1,
   EmpDag_Vi_ViConstraint          = 2,
   EmpDag_Vi_OptSolMapConstraint   = 4,
   EmpDag_Vi_MultiLevelConstraint  = 8,
} EmpDagViEdgeFeatures;

typedef struct {
   EmpDagRootType          rootnode;
   EmpDagOptEdgeFeatures   constraint;
   EmpDagViEdgeFeatures    vicons;
} EmpDagFeatures;

typedef struct {
   unsigned num_min;
   unsigned num_max;
   unsigned num_feas;
   unsigned num_vi;
   unsigned num_equil;
} EmpDagNodeStats;

typedef struct {
   unsigned num_ctrl;
   unsigned num_equil;
   unsigned num_vf;
} EmpDagEdgeStats;

/** @brief Main EMP DAG structure */
typedef struct empdag {
   EmpDagType type;
   EmpDagFeatures features;
   EmpDagNodeStats node_stats;
   EmpDagEdgeStats edge_stats;
   bool finalized;

   DagMpArray mps;
   DagMpeArray mpes;
   DagUidArray roots;

   daguid_t uid_root;

   MpIdArray mps2reformulate;
   MpIdArray saddle_path_starts;

   struct {
      RhpSense sense;
      rhp_idx objequ;
      rhp_idx objvar;
   } simple_data;                /**< Data structure for simple case  */

   Model *mdl;
   struct empdag *empdag_next;     /**< UNUSED */
   const struct empdag *empdag_up; /**< UNUSED */
} EmpDag;

/* --------------------------------------------------------------------------
 * Simple initialization procedures
 * -------------------------------------------------------------------------- */

void empdag_init(EmpDag *empdag, Model *mdl) NONNULL;
void empdag_rel(EmpDag *empdag) NONNULL;
int empdag_next(EmpDag *empdag) NONNULL;
int empdag_fini(EmpDag *empdag) NONNULL;

void mpe_free(Mpe *mpe);
Mpe *mpe_new(unsigned id, Model *mdl) MALLOC_ATTR(mpe_free, 1) NONNULL;
unsigned mpe_getid(const Mpe *mpe) NONNULL;

void empdag_reset_type(EmpDag *empdag) NONNULL;
int empdag_initfromDAG(EmpDag *empdag, const EmpDag *empdag_up,
                       Model *mdl) NONNULL;
int empdag_initfrommodel(EmpDag * restrict empdag, const Model *mdl_up) NONNULL;
int empdag_initDAGfrommodel(Model *mdl, const Avar *v_no) NONNULL;

int empdag_finalize(Model *mdl) NONNULL;
/* --------------------------------------------------------------------------
 * Node (MP, MPE) creation functions
 * -------------------------------------------------------------------------- */

NONNULL ACCESS_ATTR(write_only, 3)
int empdag_addmp(EmpDag *empdag, RhpSense sense, unsigned *id);

ACCESS_ATTR(write_only, 4) OWNERSHIP_TAKES(3)
NONNULL_ONEIDX(1) NONNULL_ONEIDX(4)
int empdag_addmpnamed(EmpDag *empdag, RhpSense sense,
                      const char *name, unsigned *id);

NONNULL
MathPrgm *empdag_newmp(EmpDag *empdag, RhpSense sense);

NONNULL_ONEIDX(1) OWNERSHIP_TAKES(3)
MathPrgm *empdag_newmpnamed(EmpDag *empdag, RhpSense sense, const char *name);

NONNULL ACCESS_ATTR(write_only, 2)
int empdag_addmpe(EmpDag *empdag, unsigned *id);

ACCESS_ATTR(write_only, 3) OWNERSHIP_TAKES(2)
NONNULL_ONEIDX(1) NONNULL_ONEIDX(3)
int empdag_addmpenamed(EmpDag *empdag, const char *name, unsigned *id);

Mpe *empdag_newmpe(EmpDag *empdag) NONNULL;
NONNULL_ONEIDX(1) OWNERSHIP_TAKES(2) 
Mpe *empdag_newmpenamed(EmpDag *empdag, const char* name);

int empdag_reserve_mp(EmpDag *empdag, unsigned reserve) NONNULL;

/* --------------------------------------------------------------------------
 * Node (MP, MPE) setting functions
 * -------------------------------------------------------------------------- */

int empdag_setmpname(EmpDag *empdag, mpid_t mpid,
                     const char *name) NONNULL;
int empdag_setmpename(EmpDag *empdag, mpeid_t mpeid,
                      const char *name) NONNULL;

/* --------------------------------------------------------------------------
 * Node (MP, MPE) getter functions
 * -------------------------------------------------------------------------- */

int empdag_getmpbyname(const EmpDag *empdag, const char *name,
                       MathPrgm **mp) ACCESS_ATTR(write_only, 3) NONNULL;
int empdag_getmpbyid(const EmpDag *empdag, unsigned id,
                     MathPrgm **mp) ACCESS_ATTR(write_only, 3) NONNULL;
int empdag_getmpidbyname(const EmpDag *empdag, const char *name,
                         unsigned *id) ACCESS_ATTR(write_only, 3) NONNULL;

int empdag_getmpebyname(const EmpDag *empdag, const char *name,
                        Mpe **mpe) ACCESS_ATTR(write_only, 3) NONNULL;
int empdag_getmpebyid(const EmpDag *empdag, unsigned id, Mpe **mpe)
                      ACCESS_ATTR(write_only, 3) NONNULL;
int empdag_getmpeidbyname(const EmpDag *empdag, const char *name,
                          unsigned *id) ACCESS_ATTR(write_only, 3);

/* --------------------------------------------------------------------------
 * Arcs related functions
 * -------------------------------------------------------------------------- */

int empdag_mpVFmpbyid(EmpDag *empdag, unsigned id_parent,
                      const struct rhp_empdag_arcVF *edgeVF) NONNULL;
int empdag_mpCTRLmpbyid(EmpDag *empdag, unsigned id_parent, unsigned id_child)
                        NONNULL;
int empdag_mpCTRLmpebyid(EmpDag *empdag, mpid_t mpid, mpeid_t mpeid)
                        NONNULL;
int empdag_mpeaddmpbyid(EmpDag *empdag, mpeid_t mpeid, mpid_t mpid)
                        NONNULL;
int empdag_mpeaddmpsbyid(EmpDag *empdag, mpeid_t mpeid,
                         const UIntArray *mps) NONNULL;

int empdag_mpVFmpbyname(EmpDag *empdag, const char *mp1_name,
                        const struct rhp_empdag_arcVF *vf_info) NONNULL;
int empdag_mpCTRLmpbyname(EmpDag *empdag, const char *mp1_name,
                          const char *mp2_name) NONNULL;
int empdag_mpCTRLmpebyname(EmpDag *empdag, const char *mp_name,
                           const char *mpe_name) NONNULL;
int empdag_mpeaddmpbyname(EmpDag *empdag, const char *mpe_name,
                          const char *mp_name) NONNULL;
int empdag_mpeaddmpsbyname(EmpDag *empdag, const char *mpe_name,
                           const char *mp_names, unsigned mps_len) NONNULL;

int empdag_addarc(EmpDag *empdag, daguid_t uid_parent, daguid_t uid_child,
                          struct empdag_edge *edge) NONNULL;
/* --------------------------------------------------------------------------
 * Roots related functions
 * -------------------------------------------------------------------------- */

int empdag_rootsaddmpe(EmpDag *empdag, mpeid_t mpeid) NONNULL;
int empdag_rootsaddmp(EmpDag *empdag, mpid_t mpid) NONNULL;
int empdag_rootsadd(EmpDag *empdag, daguid_t uid) NONNULL;
int empdag_rootset(EmpDag *empdag, daguid_t uid) NONNULL;
int empdag_collectroots(EmpDag *empdag, UIntArray *roots) NONNULL;

int empdag_getmpparents(const EmpDag *empdag, const MathPrgm *mp,
                        const UIntArray **parents_uid) NONNULL;
int empdag_getmpeparents(const EmpDag *empdag, const Mpe *mpe,
                         const UIntArray **parents_uid) NONNULL;

/* --------------------------------------------------------------------------
 * Subdag functions
 * -------------------------------------------------------------------------- */
int empdag_subdag_getmplist(const EmpDag *empdag, daguid_t subdag_root,
                            UIntArray *mplist) NONNULL;

/* --------------------------------------------------------------------------
 * Removal
 * -------------------------------------------------------------------------- */

int empdag_delete(EmpDag *empdag, daguid_t uid) NONNULL;

/* --------------------------------------------------------------------------
 * Getters
 * -------------------------------------------------------------------------- */

int empdag_mpe_getchildren(const EmpDag *empdag, mpeid_t mpeid,
                           UIntArray *mps) WRITE_ONLY(3) NONNULL;

bool empdag_mphasname(const EmpDag *empdag, mpid_t mpid);
const char *empdag_getmpname(const EmpDag *empdag, mpid_t mpid);
const char* empdag_getmpname2(const EmpDag *empdag, mpid_t mpid);
const char *empdag_getmpename(const EmpDag *empdag, mpeid_t mpeid);
unsigned empdag_getmpcurid(const EmpDag *empdag, MathPrgm *mp) NONNULL;
const struct rhp_empdag_arcVF* empdag_find_edgeVF(const EmpDag *empdag, mpid_t mpid_parent,
                                            mpid_t mpid_child) NONNULL;

/* --------------------------------------------------------------------------
 * Parsing
 * -------------------------------------------------------------------------- */

int empdag_empfile_parse(Model *mdl, int *addr);

/* --------------------------------------------------------------------------
 * Analysis
 * -------------------------------------------------------------------------- */

int empdag_check(EmpDag *empdag);

/* --------------------------------------------------------------------------
 * Transformation
 * -------------------------------------------------------------------------- */

int empdag_minimax_reformulate(Model *mdl) NONNULL;


static inline bool empdag_mphaschild(const EmpDag *empdag, mpid_t mpid) {
   assert(mpid < empdag->mps.len);
   return empdag->mps.Carcs[mpid].len > 0;
}


NONNULL static inline bool empdag_rootismpe(const EmpDag *empdag) {
   return empdag->roots.len == 1 && uidisMPE(empdag->roots.arr[0]);
}

 NONNULL static inline bool empdag_rootismp(const EmpDag *empdag) {
   return empdag->roots.len == 1 && uidisMP(empdag->roots.arr[0]);

}

int empdag_simple_init(EmpDag *empdag);
int empdag_simple_setsense(EmpDag *empdag, RhpSense sense);
int empdag_simple_setobjequ(EmpDag *empdag, rhp_idx objequ);
int empdag_simple_setobjvar(EmpDag *empdag, rhp_idx objvar);

int empdag_single_MP_to_Nash(EmpDag* empdag) NONNULL;

/* --------------------------------------------------------------------------
 * Misc (printing and exporting)
 * -------------------------------------------------------------------------- */

const char* empdag_printid(const EmpDag *empdag) NONNULL;
const char* empdag_getname(const EmpDag *empdag, unsigned uid) NONNULL;
const char* empdag_printmpid(const EmpDag *empdag, mpid_t mpid) NONNULL;
const char* empdag_printmpeid(const EmpDag *empdag, mpeid_t mpeid) NONNULL;
const char* empdag_typename(EmpDagType type);


int empdag_exportasdot(Model *mdl) NONNULL;
int empdag2dotfile(const EmpDag* empdag, const char* fname) NONNULL;
int empdag2dotenvname(const EmpDag* empdag, const char *envname) NONNULL;

/* --------------------------------------------------------------------------
 * Fast inline stuff
 * -------------------------------------------------------------------------- */

static inline MathPrgm* empdag_getmpfast(const EmpDag *empdag, mpid_t mpid)
{
   assert(mpid < empdag->mps.len);
   return empdag->mps.arr[mpid];
}

static inline unsigned empdag_getmplen(const EmpDag *empdag)
{
   return empdag->mps.len;
}

static inline Mpe* empdag_getmpefast(const EmpDag *empdag, mpeid_t mpeid)
{
   assert(mpeid < empdag->mpes.len);
   return empdag->mpes.arr[mpeid];
}

static inline unsigned empdag_getmpelen(const EmpDag *empdag)
{
   return empdag->mpes.len;
}

static inline bool empdag_isroot(const EmpDag *empdag, unsigned uid)
{
   return rhp_uint_findsorted(&empdag->roots, uid) != UINT_MAX;
}

static inline unsigned empdag_getrootuidfast(const EmpDag *empdag, unsigned root_id)
{
   assert(root_id < empdag->roots.len);
   return empdag->roots.arr[root_id];
}

static inline unsigned empdag_getrootlen(const EmpDag *empdag)
{
   return empdag->roots.len;
}

static inline bool empdag_exists(const EmpDag *empdag) {
   return empdag->mps.len > 0;
}

static inline bool empdag_isempty(const EmpDag *empdag) {
   assert(empdag->type != EmpDag_Unset);
   return empdag->type == EmpDag_Empty;
}

static inline bool empdag_singleprob(const EmpDag *empdag) {
  assert(empdag->type != EmpDag_Unset);

  switch (empdag->type) {
  case EmpDag_Single_Opt:
  case EmpDag_Single_Vi:
    return true;
  default:
    return false;
  }
}

static inline bool empdag_isopt(const EmpDag *empdag) {
   assert(empdag->type != EmpDag_Unset);
   return empdag->type == EmpDag_Single_Opt;
}

static inline bool empdag_isset(const EmpDag *empdag) {
   return empdag->type != EmpDag_Unset;
}

static inline void empdag_setrootprobtype(EmpDag *empdag, EmpDagRootType feature)
{
   empdag->features.rootnode = feature;
}

static inline void empdag_opt_consfeature(EmpDag *empdag, EmpDagOptEdgeFeatures feature)
{
   empdag->features.constraint |= feature;
}

static inline void empdag_vi_consfeature(EmpDag *empdag, EmpDagViEdgeFeatures feature)
{
   empdag->features.vicons |= feature;
}

ACCESS_ATTR(write_only, 3) NONNULL
static inline int empdag_getmpbyuid(const EmpDag *empdag, unsigned uid,
                                    MathPrgm **mp) {
   assert(uidisMP(uid));
   return empdag_getmpbyid(empdag, uid2id(uid), mp);
}

ACCESS_ATTR(write_only, 3) NONNULL
static inline int empdag_getmpebyuid(const EmpDag *empdag, unsigned uid,
                                     Mpe **mpe) {
   assert(uidisMPE(uid));
   return empdag_getmpebyid(empdag, uid2id(uid), mpe);
}

static inline bool empdag_has_adversarial_mps(const EmpDag *empdag) {
   return empdag->mps2reformulate.len > 0;
}



#endif /* EMPDAG_H */
