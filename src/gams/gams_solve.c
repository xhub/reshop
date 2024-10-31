#include "ctrdat_gams.h"
#include "fooc.h"
#include "gams_solve.h"
#include "gams_macros.h"
#include "gams_rosetta.h"
#include "instr.h"
#include "mathprgm.h"
#include "mdl.h"
#include "mdl_gams.h"
#include "mdl_rhp.h"
#include "rhp_model.h"
#include "rmdl_gams.h"
#include "var.h"


#include "gmomcc.h"
#include "gevmcc.h"


UNUSED static int err_mdlbackend_gmo(Model *mdl)
{
   error("[model] ERROR: can't create a GMO for %s model '%.*s' #%u\n",
         mdl_fmtargs(mdl));
   return Error_NotImplemented;
}

// TODO: this should be added somewhere else
//   if (nvars > INT_MAX || nequs > INT_MAX) {
//      error("[gams] ERROR: %s model '%.*s' #%u is too big for GMO: nvars = %u; "
//            "nequs = %u. Max values for both is %d\n", mdl_fmtargs(mdl), nvars,
//            nequs, INT_MAX);
//      return Error_
//   }
static int gmdl_gmo2gmo_fops(Model *mdl, Model *mdl_dst, Fops* fops)
{
   assert(mdl->backend == RHP_BACKEND_GAMS_GMO && mdl_dst->backend == RHP_BACKEND_GAMS_GMO);
   UNUSED char buffer[GMS_SSSIZE];

   GmsContainerData *gmsdst = mdl_dst->ctr.data;
   GmsContainerData *gmssrc = mdl->ctr.data;
   UNUSED gmoHandle_t gmodst = gmsdst->gmo;
   UNUSED gmoHandle_t gmosrc = gmssrc->gmo;

   /* Dummy */
   int nvars = 0, nequs = 0;

   dctHandle_t dctdst = gmsdst->dct;
   dctHandle_t dctsrc = gmssrc->dct;
   int nuels = dctNUels(dctsrc);

   dctSetBasicCounts(dctdst, nvars, nequs, nuels);

   return OK;
}

static int gmdl_gmo2gmo(Model *mdl, Model *mdl_dst)
{
   assert(mdl->backend == RHP_BACKEND_GAMS_GMO && mdl_dst->backend == RHP_BACKEND_GAMS_GMO);
   char buffer[GMS_SSSIZE];

   S_CHECK(gmdl_cdat_create(mdl_dst, mdl));

   GmsContainerData *gmsdst = mdl_dst->ctr.data;
   GmsContainerData *gmssrc = mdl->ctr.data;
   gmoHandle_t gmodst = gmsdst->gmo;
   gmoHandle_t gmosrc = gmssrc->gmo;

  /* ----------------------------------------------------------------------
   * We copy the container content without translation
   * ---------------------------------------------------------------------- */

   gmoPinfSet(gmodst, gmoPinf(gmosrc));
   gmoMinfSet(gmodst, gmoMinf(gmosrc));
   gmoIndexBaseSet(gmodst, gmoIndexBase(gmosrc));

   int nvars = gmoN(gmosrc);
   int nequs = gmoM(gmosrc);

   int nlcode_maxlen = gmoNLCodeSizeMaxRow(gmosrc);
   int *colidxs, *nlflags, *opcodes, *fields;

   size_t memsize_int = sizeof(int) * 2 * (nvars + nlcode_maxlen);
   size_t memsize = memsize_int + sizeof(double) * nvars;

   struct ctrmem CTRMEM working_mem = {.ptr = NULL, .ctr = &mdl->ctr};
   A_CHECK(working_mem.ptr, ctr_getmem(&mdl->ctr, memsize));
   double *jacvals = (double*) working_mem.ptr;
   colidxs = (int*)&jacvals[nvars];
   nlflags = &colidxs[nvars];
   opcodes = &nlflags[nvars];
   fields = &opcodes[nlcode_maxlen];

   int nlpool_size = 0;
   double *nlpool = NULL;
   if (nlcode_maxlen > 0) {
      /* TODO: GAMS review: document why this is needed */
      GMSCHK(gmoSetNLObject(gmodst, NULL, NULL));
      nlpool_size = gmoNLConst(gmosrc);
      nlpool = gmoPPool(gmosrc);
   }

   /* ---------------------------------------------------------------------
    * Add variables
    * --------------------------------------------------------------------- */

   for (int idx = 0; idx < nvars; ++idx) {
      GMSCHK(gmoAddCol(
                 gmodst,
                 gmoGetVarTypeOne(gmosrc, idx),    /* type of variable       */
                 gmoGetVarLowerOne(gmosrc, idx),   /* lower bound            */
                 gmoGetVarLOne(gmosrc, idx),       /* level value            */
                 gmoGetVarUpperOne(gmosrc, idx),   /* upper bound            */
                 gmoGetVarMOne(gmosrc, idx),       /* marginal value         */
                 (gmoGetVarStatOne(gmosrc, idx) == gmoBstat_Basic) ? 0 : 1,
                                                   /* basis flag (0=basic)   */
                 gmoGetVarSosSetOne(gmosrc, idx),  /* SOS set variable       */
                 gmoGetVarPriorOne(gmosrc, idx),   /* priority of variable   */
                 gmoGetVarScaleOne(gmosrc, idx),   /* scale of variable      */
                 0,                                /* nnz  (unused)          */
                 NULL,                             /* rowidx of Jacobian     */
                 NULL,                             /* jacval of Jacobian     */
                 NULL));                           /* nlflag of Jacobian     */
   }

   /* ---------------------------------------------------------------------
    * Add equations
    * --------------------------------------------------------------------- */

   int offset_match = 1 - gmoIndexBase(gmosrc);
   assert(offset_match >= 0 && offset_match <= 1); 

   for (int idx = 0; idx < nequs; ++idx) {

      /* ------------------------------------------------------------------
       * Get the equation content
       * ------------------------------------------------------------------ */

      int nnz, nnzNL;
      GMSCHK(gmoGetRowSparse(gmosrc, idx, colidxs, jacvals, nlflags, &nnz, &nnzNL));
      int match = gmoGetEquMatchOne(gmosrc, idx) + offset_match;

      GMSCHK(gmoAddRow(
                gmodst,
                gmoGetEquTypeOne(gmosrc, idx),    /* type of equation       */
                match,                            /* index of matching var  */
                gmoGetEquSlackOne(gmosrc, idx),   /* slack value            */
                gmoGetEquScaleOne(gmosrc, idx),   /* scale of equation      */
                gmoGetRhsOne(gmosrc, idx),        /* right-hand side value  */
                gmoGetEquMOne(gmosrc, idx),       /* marginal value         */
                (gmoGetEquStatOne(gmosrc, idx) == gmoBstat_Basic) ? 0 : 1,
                                                  /* basis flag (0=basic)   */
                nnz,                               /* nnz in equ             */
                colidxs,                          /* colidx of Jacobian     */
                jacvals,                          /* jacval of Jacobian     */
                nlflags));                        /* nlflag of Jacobian     */

      /* ------------------------------------------------------------------
       * Copy nonlinear instructions.
       * ------------------------------------------------------------------ */

      int instr_len;
      GMSCHK(gmoDirtyGetRowFNLInstr(gmosrc, idx, &instr_len, opcodes, fields));

      if (instr_len > 0) {
         assert(nlcode_maxlen > 0);

//         /* ---------------------------------------------------------------
//          * When the opcode involves variable index, update it into the
//          * new order.
//          * --------------------------------------------------------------- */
//
//         for (int j = 0; j < num_instr; ++j) {
//            switch (opcodes[j]) {
//            case nlPushV:
//            case nlAddV:
//            case nlSubV:
//            case nlMulV:
//            case nlDivV:
//            case nlUMinV:
//               fields[j] = cperm[fields[j]-1] + 1;
//               break;
//            case nlStore:
//               fields[j] = idx+1;
//               break;
//            default:
//               break;
//            }
//         }

         /* The pool is only set at the first call, yet pass it all the times */
         GMSCHK(gmoDirtySetRowFNLInstr(gmodst, idx, instr_len, opcodes, fields,
                                       NULL, nlpool, nlpool_size));
      }

   }

   int sense = gmoSense(gmosrc);
   if (sense == gmoObj_Max || sense == gmoObj_Min) {

      gmoSenseSet(gmodst, sense);
      int objstyle = gmoObjStyle(gmosrc);
      gmoObjStyleSet(gmodst, objstyle);

      if (objstyle == gmoObjType_Var) {
         gmoObjVarSet(gmodst, gmoObjVar(gmosrc));
      } else if (objstyle == gmoObjType_Fun) {
         TO_IMPLEMENT("Objstyle is gmoObjType_Fun");
      } else {
         error("[GAMS] ERROR in %s model '%.*s' #%u: invalid GMO objstyle %d\n",
               mdl_fmtargs(mdl), objstyle);
         return Error_RuntimeError;
      }
      gmoObjReformSet(gmodst, gmoObjReform(gmosrc));
   }

  /* ----------------------------------------------------------------------
   * Reuse the dictionary
   * ---------------------------------------------------------------------- */

   gmoDictSet(gmodst, gmoDict(gmosrc));
   gmoDictionarySet(gmodst, gmoDictionary(gmosrc));

   gmoPriorOptSet(gmodst, gmoPriorOpt(gmosrc));
   gmoScaleOptSet(gmodst, gmoScaleOpt(gmosrc));
   gmoHaveBasisSet(gmodst, gmoHaveBasis(gmosrc));
   gmoIsMPSGESet(gmodst, gmoIsMPSGE(gmosrc));


   GMSCHK(gmoCompleteData(gmodst, buffer));

   return OK;
}

int gmdl_creategmo(Model *mdl, Model *mdl_solver)
{
   assert(mdl->backend == RHP_BACKEND_GAMS_GMO);
   assert(mdl_solver->backend == RHP_BACKEND_GAMS_GMO);

   if (!(mdl_solver->status & MdlInstantiable)) {
      error("[GMOexport] ERROR: %s model '%.*s' #%u is not instantiable\n",
            mdl_fmtargs(mdl_solver));
      return Error_RuntimeError;
   }

   Fops * fops = mdl->ctr.fops;
   if (fops) {
      return gmdl_gmo2gmo_fops(mdl, mdl_solver, fops);
   }

   return gmdl_gmo2gmo(mdl, mdl_solver);
}

