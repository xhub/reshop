#include "reshop_config.h"

#include <string.h>

#include "compat.h"
#include "empdag.h"
#include "empinfo.h"
#include "empinterp_utils.h"
#include "empinterp_vm.h"
#include "empinterp_vm_utils.h"
#include "macros.h"
#include "mathprgm.h"
#include "mdl.h"
#include "ovf_parameter.h"
#include "printout.h"
#include "status.h"

static int _argcnt(unsigned argc, unsigned arity, const char *fn)
{
   if (argc != arity) {
      error("%s :: ERROR: expecting %u arguments, got %u.\n", fn, arity, argc);
      return Error_RuntimeError;
   }
   return OK;
}

static inline int chk_param_idx(unsigned idx, unsigned size, const char *fn)
{
   if (idx >= size) {
      error("%s :: param index %u >= %u, the number of parameters", fn, idx, size);
      return Error_EMPRuntimeError;
   }

   return OK;
}

/* ------------------------------------------------------------------------
 *  Small utility functions used when creating and finalizing an empdag
 * ------------------------------------------------------------------------ */

NONNULL_AT(1) static
int vm_common_nodeinit(VmData *data, daguid_t uid, DagRegisterEntry *regentry)
{
   if (regentry) {
      regentry->daguid_parent = uid;
      DagRegisterEntry *regentry_;
      A_CHECK(regentry_, regentry_dup(regentry));

      dagregister_add(data->dagregister, regentry_);
   }

   if (valid_uid(data->uid_grandparent)) {
      errormsg("[empvm] ERROR: grandparent uid is valid, please file a bug\n");
      return Error_EMPRuntimeError;
   }

   data->uid_grandparent = data->uid_parent;
   data->uid_parent = uid;

   return OK;
}

NONNULL static int vm_common_nodefini(VmData *data)
{
   data->uid_parent = data->uid_grandparent;
   data->uid_grandparent = EMPDAG_UID_NONE;

   return OK;
}


/* ------------------------------------------------------------------------
 *  Functions creating a new object
 * ------------------------------------------------------------------------ */

static void* ccflib_newobj(VmData *data, unsigned argc, const VmValue *values)
{
   SN_CHECK(_argcnt(argc, 2, __func__));
   EmpDag *empdag = &data->mdl->empinfo.empdag;
   assert(IS_UINT(values[0]));
   unsigned ccflib_idx = AS_UINT(values[0]);

   VmValue stack2 = values[1];
   DagRegisterEntry *regentry;
   char *label;
   if (IS_NIL(stack2)) {
      label = NULL;
      regentry = NULL;

   } else if (IS_REGENTRY(stack2)) {
      regentry = AS_REGENTRY(stack2);
      genlabelname(regentry, data->interp, &label);

   } else {
      error("[empvm_run] ERROR in %s: 2nd argment has the wrong type. Please "
            "report this bug.\n", __func__);
      return NULL;
   }

   MathPrgm *mp;
   AA_CHECK(mp, empdag_newmpnamed(empdag, RhpNoSense, label));
   SN_CHECK(mp_from_ccflib(mp, ccflib_idx));

   SN_CHECK(vm_common_nodeinit(data, mpid2uid(mp->id), regentry));

   free(label);

   return mp;
}

static void* mp_newobj(VmData *data, unsigned argc, const VmValue *values)
{
   SN_CHECK(_argcnt(argc, 2, __func__));
   EmpDag *empdag = &data->mdl->empinfo.empdag;
   assert(IS_UINT(values[0]));
   RhpSense sense = AS_UINT(values[0]);

   VmValue stack2 = values[1];
   DagRegisterEntry *regentry;
   char *label;
   if (IS_NIL(stack2)) {
      label = NULL;
      regentry = NULL;

   } else if (IS_REGENTRY(stack2)) {
      regentry = AS_REGENTRY(stack2);
      genlabelname(regentry, data->interp, &label);

   } else {
      error("[empvm_run] ERROR in %s: 2nd argment has the wrong type. Please "
            "report this bug.\n", __func__);
      return NULL;
   }

   MathPrgm *mp;
   AA_CHECK(mp, empdag_newmpnamed(empdag, sense, label));

   SN_CHECK(vm_common_nodeinit(data, mpid2uid(mp->id), regentry));

   free(label);

   return mp;
}

static void* nash_newobj(VmData *data, unsigned argc, const VmValue *values)
{
   SN_CHECK(_argcnt(argc, 1, __func__));
   EmpDag *empdag = &data->mdl->empinfo.empdag;
   DagRegisterEntry *regentry;
   char *label;
   VmValue stackval = values[0];

   if (IS_NIL(stackval)) {
      label = NULL;
      regentry = NULL;

   } else if (IS_REGENTRY(stackval)) {
      regentry = AS_REGENTRY(stackval);
      genlabelname(regentry, data->interp, &label);

   } else {
      error("[empvm_run] ERROR in %s: 2nd argument has the wrong type. Please "
            "report this bug.\n", __func__);
      return NULL;
   }

   Nash *nash;
   AA_CHECK(nash, empdag_newnashnamed(empdag, label));

   SN_CHECK(vm_common_nodeinit(data, nashid2uid(nash->id), regentry));

   return nash;
}

static void* ovf_newobj(VmData *data, unsigned argc, const VmValue *values)
{
   SN_CHECK(_argcnt(argc, 1, __func__));
   assert(IS_STR(values[0]));
   const char *name = AS_STR(values[0]);

   OvfDef *ovfdef;
   SN_CHECK(ovf_addbyname(&data->mdl->empinfo, name, &ovfdef));

   return ovfdef;
}

/* ------------------------------------------------------------------------
 *  CCFLIB API functions
 * ------------------------------------------------------------------------ */


static int vm_ccflib_finalize(VmData *data, unsigned argc, const VmValue *values)
{
   assert(IS_MPOBJ(values[0]));
   assert(IS_MPOBJ(values[-1]));
   S_CHECK(_argcnt(argc, 1, __func__));
   MathPrgm *mp, *mp_parent;
   N_CHECK(mp, AS_MPOBJ(values[0]));

   /* ---------------------------------------------------------------------
    * Is the CCF has no child, by convention it has value 1 and we delete the MP
    * --------------------------------------------------------------------- */

   EmpDag *empdag = &data->mdl->empinfo.empdag;
   unsigned len = data->linklabels2arcs->len;
   daguid_t ccflib_uid = mpid2uid(mp->id);
   bool has_child = false;
   unsigned num_children = 0;

   if (len > 0) {
      LinkLabels *linklabels = data->linklabels2arcs->arr[len-1];
      assert(linklabels->num_children > 0);
      has_child = linklabels->daguid_parent == ccflib_uid;
      if (has_child) { num_children = linklabels->num_children; }
   }

   len = data->linklabel2arc->len;
   if (!has_child && len > 0) { 
      LinkLabel *linklabel_last = data->linklabel2arc->arr[len-1];
      has_child = linklabel_last->daguid_parent == ccflib_uid;
      if (has_child) { num_children = 1; }
   }

   if (!has_child) {
      S_CHECK(empdag_delete(empdag, ccflib_uid));

      return OK;
   }

   N_CHECK(mp_parent, AS_MPOBJ(values[-1]));

   ArcVFObj obj = {.id_parent = mp_parent->id, .id_child = mp->id};

   S_CHECK(arcvfobjs_add(&data->arcvfobjs, obj))

   assert(mp->ccflib.ccf);

   /* Update the number of children */
   mp->ccflib.ccf->num_empdag_children = num_children;

   S_CHECK(mp_finalize(mp));

   if (O_Output & PO_TRACE_EMPINTERP) {
      ovf_def_print(mp->ccflib.ccf, PO_TRACE_EMPINTERP, data->mdl);
   }

   return vm_common_nodefini(data);
}

static int vm_ccflib_updateparams(UNUSED VmData *data, unsigned argc, const VmValue *values)
{
   assert(IS_MPOBJ(values[0]) && IS_PTR(values[1]));
   S_CHECK(_argcnt(argc, 2, __func__));

   MathPrgm *mp;
   N_CHECK(mp, AS_MPOBJ(values[0]));
   assert(mp->type == MpTypeCcflib);

   OvfParamList *params;
   N_CHECK(params, (OvfParamList *)AS_PTR(values[1]));

   return ovf_params_sync(mp->ccflib.ccf, params);
}


/* ------------------------------------------------------------------------
 *  MP API functions
 * ------------------------------------------------------------------------ */



static int vm_mp_addcons(VmData *data, unsigned argc, const VmValue *values)
{
   S_CHECK(_argcnt(argc, 1, __func__));
   MathPrgm *mp;
   assert(IS_MPOBJ(values[0]));
   N_CHECK(mp, (MathPrgm *)AS_MPOBJ(values[0]));
   Aequ *e = data->e_current;

   return mp_addconstraints(mp, e);
}

static int vm_mp_addvars(VmData *data, unsigned argc, const VmValue *values)
{
   S_CHECK(_argcnt(argc, 1, __func__));
   MathPrgm *mp;
   assert(IS_MPOBJ(values[0]));
   N_CHECK(mp, (MathPrgm *)AS_MPOBJ(values[0]));
   Avar *v = data->v_current;

   return mp_addvars(mp, v);
}

static int vm_mp_addvipairs(VmData *data, unsigned argc, const VmValue *values)
{
   S_CHECK(_argcnt(argc, 1, __func__));
   MathPrgm *mp;
   assert(IS_MPOBJ(values[0]));
   N_CHECK(mp, (MathPrgm *)AS_MPOBJ(values[0]));
   Avar *v = data->v_current;
   Aequ *e = data->e_current;

   return mp_addvipairs(mp, e, v);
}

static int vm_mp_addzerofunc(VmData *data, unsigned argc, const VmValue *values)
{
   S_CHECK(_argcnt(argc, 1, __func__));
   MathPrgm *mp;
   assert(IS_MPOBJ(values[0]));
   N_CHECK(mp, (MathPrgm *)AS_MPOBJ(values[0]));
   Avar *v = data->v_current;

   return mp_addvipairs(mp, NULL, v);
}

static int vm_mp_finalize(UNUSED VmData *data, unsigned argc, const VmValue *values)
{
   S_CHECK(_argcnt(argc, 1, __func__));
   MathPrgm *mp;
   assert(IS_MPOBJ(values[0]));
   N_CHECK(mp, (MathPrgm *)AS_MPOBJ(values[0]));

   /* ---------------------------------------------------------------------
    * We first call finalize as it identifies the objective function
    * --------------------------------------------------------------------- */

   mp_finalize(mp);

   /* ---------------------------------------------------------------------
    * If there were VF functions in the objective, we now add the
    * corresponding EMPDAG edges. We need to do this as 
    * --------------------------------------------------------------------- */

   unsigned num_arcvfobj = data->arcvfobjs.len;
   if (num_arcvfobj > 0) {
      mpid_t mp_id = mp->id;
      ArcVFObj *o = data->arcvfobjs.arr;
      rhp_idx objequ = mp_getobjequ(mp);
      assert(valid_ei(objequ)); // TODO support the case where there is no objequ
      
      EmpDag *empdag = &data->mdl->empinfo.empdag;

      ArcVFData arcvf;
      arcVFb_init(&arcvf, objequ);

      for (unsigned i = 0; i < num_arcvfobj; ++i, o++) {
         assert(o->id_parent == mp_id);

         arcvf.mpid_child = o->id_child;
         S_CHECK(empdag_mpVFmpbyid(empdag, mp_id, &arcvf));

      }
      data->arcvfobjs.len = 0;
   }

   return vm_common_nodefini(data);
}

static int vm_mp_setprobtype(UNUSED VmData *data, unsigned argc, const VmValue *values)
{
   S_CHECK(_argcnt(argc, 2, __func__));
   MathPrgm *mp;
   assert(IS_MPOBJ(values[0]));
   N_CHECK(mp, (MathPrgm *)AS_MPOBJ(values[0]));
   unsigned type = AS_UINT(values[1]);

   return mp_setprobtype(mp, type);
}

static int vm_mp_setobjvar(UNUSED VmData *data, unsigned argc, const VmValue *values)
{
   S_CHECK(_argcnt(argc, 1, __func__));
   MathPrgm *mp;
   assert(IS_MPOBJ(values[0]));
   N_CHECK(mp, (MathPrgm *)AS_MPOBJ(values[0]));

   Avar *v = data->v_current;
   unsigned dim = avar_size(v);

   if (dim != 1) {
      error("[empvm_run] ERROR: expecting a scalar objective variable, got dim = %u.\n", dim);
      return Error_EMPIncorrectInput;
   }


   return mp_setobjvar(mp, avar_fget(v, 0));
}

static int vm_nash_addmpbyid(UNUSED VmData *data, unsigned argc, const VmValue *values)
{
   S_CHECK(_argcnt(argc, 2, __func__));
   MathPrgm *mp;
   assert(IS_MPOBJ(values[0]));
   N_CHECK(mp, (MathPrgm *)AS_MPOBJ(values[0]));

   assert(IS_UINT(values[1]));
   unsigned equil_id = AS_UINT(values[1]);

   return empdag_nashaddmpbyid(&data->mdl->empinfo.empdag, equil_id, mp->id);
}

static int vm_nash_finalize(UNUSED VmData *data, unsigned argc, const VmValue *values)
{
   assert(_argcnt(argc, 1, __func__) == OK);
   assert(IS_NASHOBJ(values[0]));
   /* No need to dig the object for now */
   //S_CHECK(_argcnt(argc, 1, __func__));
   //Mpe *mpe;
   //N_CHECK(mpe, (Mpe *)AS_NASHOBJ(values[0]));

   return vm_common_nodefini(data);
}

/* ------------------------------------------------------------------------
 *  OVF API functions
 * ------------------------------------------------------------------------ */

static int vm_ovf_setrho(VmData *data, unsigned argc, const VmValue *values)
{
   assert(IS_OVFOBJ(values[0]));
   S_CHECK(_argcnt(argc, 1, __func__));
   OvfDef *ovfdef;
   N_CHECK(ovfdef, AS_OVFOBJ(values[0]));
   Avar *v = data->v_current;
   unsigned rho_size = avar_size(v);

   if (rho_size != 1) {
      error("%s :: rho variable should have size 1, got %u\n",
            __func__,  rho_size);
      return Error_EMPRuntimeError;
   }

   ovfdef->vi_ovf = avar_fget(v, 0);

   return OK;
}

static int vm_ovf_addarg(VmData *data, unsigned argc, const VmValue *values)
{
   assert(IS_OVFOBJ(values[0]));
   S_CHECK(_argcnt(argc, 1, __func__));
   OvfDef *ovfdef;
   N_CHECK(ovfdef, AS_OVFOBJ(values[0]));
   Avar *v = data->v_current;

   return avar_extend(ovfdef->args, v);
}

static int vm_ovf_finalize(VmData *data, unsigned argc, const VmValue *values)
{
   assert(IS_OVFOBJ(values[0]));
   S_CHECK(_argcnt(argc, 1, __func__));
   OvfDef *ovfdef;
   N_CHECK(ovfdef, AS_OVFOBJ(values[0]));

   S_CHECK(ovf_check(ovfdef));

   if (O_Output & PO_TRACE_EMPINTERP) {
      ovf_def_print(ovfdef, PO_TRACE_EMPINTERP, data->mdl);
   }

   return OK;
}

/**
 * @brief  Synchronize the parameter in the stack with the on in the OVF
 *
 * Rational: since we may have some hardcoded parameters in the empinfo file,
 * at compile time, we instantiate a list of OVF parameters and set the 
 * constant values there.
 *
 * @param VmData 
 * @param argc 
 * @param values 
 * @return 
 */
static int vm_ovf_syncparams(UNUSED VmData *data, unsigned argc, const VmValue *values)
{
   assert(IS_OVFOBJ(values[0]) && IS_PTR(values[1]));
   S_CHECK(_argcnt(argc, 2, __func__));

   OvfDef *ovf;
   N_CHECK(ovf, AS_OVFOBJ(values[0]));

   OvfParamList *params;
   N_CHECK(params, (OvfParamList *)AS_PTR(values[1]));

   return ovf_params_sync(ovf, params);
}

static int vm_ovfparam_setdefault(UNUSED VmData *data, unsigned argc, const VmValue *values)
{
   assert(IS_OVFOBJ(values[0]) && IS_PTR(values[1]) && IS_UINT(values[2]));
   S_CHECK(_argcnt(argc, 3, __func__));

   OvfDef *ovf;
   OvfParamList *params;
   N_CHECK(ovf, AS_OVFOBJ(values[0]));
   N_CHECK(params, (OvfParamList *)AS_PTR(values[1]));

   unsigned idx = AS_UINT(values[2]);

   S_CHECK(chk_param_idx(idx, params->size, __func__));
   OvfParam * restrict param = &params->p[idx];

   assert(param->def->default_val);
   unsigned nargs = ovf_argsize(ovf);

   return param->def->default_val(param, nargs);
}

static int vm_ovfparam_update(UNUSED VmData *data, unsigned argc, const VmValue *values)
{
   assert(IS_PTR(values[0]) && IS_UINT(values[1]) && IS_PTR(values[2]));
   S_CHECK(_argcnt(argc, 3, __func__));

   OvfParamList *params;
   N_CHECK(params, (OvfParamList *)AS_PTR(values[0]));

   unsigned idx = AS_UINT(values[1]);

   OvfParam * restrict param_update;
   N_CHECK(param_update, (OvfParam*)AS_PTR(values[2]));



   S_CHECK(chk_param_idx(idx, params->size, __func__));
   OvfParam * restrict param = &params->p[idx];

   param->type = param_update->type;
   param->size_vector = param_update->size_vector;

   switch (param->type) {
   case ARG_TYPE_SCALAR:
      param->val = param_update->val;
      break;
   case ARG_TYPE_VEC:
      param->vec = param_update->vec;
      break;
   case ARG_TYPE_MAT:
      param->mat = param_update->mat;
      break;
   default:
      error("%s :: unsupported param type %d", __func__, param->type);
      return Error_EMPRuntimeError;
   }

   if (O_Output & PO_TRACE_EMPINTERP) {
      ovf_param_print(param, PO_TRACE_EMPINTERP);
   }

   return OK;
}


/* ------------------------------------------------------------------------
 *  Misc functions
 * ------------------------------------------------------------------------ */

static int vm_chk_scalar_var(Avar *v)
{
   unsigned dim = avar_size(v);

   if (dim != 1) {
      error("[empvm_run] ERROR: expecting a scalar variable, got dim = %u.\n", dim);
      return Error_EMPIncorrectInput;
   }

   return OK;
}

static int vm_mark_equ_as_flipped(VmData *data, unsigned argc, UNUSED const VmValue *values)
{
   S_CHECK(_argcnt(argc, 0, __func__));
   Aequ *e = data->e_current;

   return ctr_markequasflipped(&data->mdl->ctr, e);
}

static int vm_mp_add_map_objfn(VmData *data, unsigned argc, UNUSED const VmValue *values)
{
   S_CHECK(_argcnt(argc, 1, __func__));
   MathPrgm *mp;
   assert(IS_MPOBJ(values[0]));
   N_CHECK(mp, (MathPrgm *)AS_MPOBJ(values[0]));

   Avar *v = data->v_current;

   S_CHECK(vm_chk_scalar_var(v));

//   S_CHECK(empdag_add_map_objfn(&data->mdl->empinfo.empdag, mpid, vi, cst));

   return OK;

}


const EmpApiCall empapis[] = {
   [FN_CCFLIB_FINALIZE]       = { .fn = vm_ccflib_finalize, .argc = 1 },
   [FN_CCFLIB_UPDATEPARAMS]   = { .fn = vm_ccflib_updateparams, .argc = 2 },
   [FN_CTR_MARKEQUASFLIPPED]  = { .fn = vm_mark_equ_as_flipped, .argc = 0},
   [FN_MP_ADDCONS]            = { .fn = vm_mp_addcons, .argc = 1 },
   [FN_MP_ADDVARS]            = { .fn = vm_mp_addvars, .argc = 1 },
   [FN_MP_ADDVIPAIRS]         = { .fn = vm_mp_addvipairs, .argc = 1 },
   [FN_MP_ADDZEROFUNC]        = { .fn = vm_mp_addzerofunc, .argc = 1 },
   [FN_MP_ADD_MAP_OBJFN]      = { .fn = vm_mp_add_map_objfn, .argc = 1},
   [FN_MP_FINALIZE]           = { .fn = vm_mp_finalize, .argc = 1 },
   [FN_MP_SETOBJVAR]          = { .fn = vm_mp_setobjvar, .argc = 1 },
   [FN_MP_SETPROBTYPE]        = { .fn = vm_mp_setprobtype, .argc = 2 },
   [FN_NASH_ADDMPBYID]        = { .fn = vm_nash_addmpbyid, .argc = 2 },
   [FN_NASH_FINALIZE]         = { .fn = vm_nash_finalize, .argc = 1 },
   [FN_OVF_ADDARG]            = { .fn = vm_ovf_addarg, .argc = 1 },
   [FN_OVF_FINALIZE]          = { .fn = vm_ovf_finalize, .argc = 1 },
   [FN_OVF_SETRHO]            = { .fn = vm_ovf_setrho, .argc = 1 },
   [FN_OVF_UPDATEPARAMS]      = { .fn = vm_ovf_syncparams, .argc = 2 },
   [FN_OVFPARAM_SETDEFAULT]   = { .fn = vm_ovfparam_setdefault, .argc = 3 },
   [FN_OVFPARAM_UPDATE]       = { .fn = vm_ovfparam_update, .argc = 3 },
};

const char * const empapis_names[] = {
   [FN_CCFLIB_FINALIZE]       =  "ccflib_finalize",
   [FN_CCFLIB_UPDATEPARAMS]   =  "ccflib_updateparams",
   [FN_CTR_MARKEQUASFLIPPED]  =  "ctr_markequasflipped",
   [FN_MP_ADDCONS]            =  "mp_addcons",
   [FN_MP_ADDVARS]            =  "mp_addvars",
   [FN_MP_ADDVIPAIRS]         =  "mp_addvipairs",
   [FN_MP_ADDZEROFUNC]        =  "mp_addzerofunc",
   [FN_MP_ADD_MAP_OBJFN]      =  "mp_add_map_objfn",
   [FN_MP_FINALIZE]           =  "mp_finalize",
   [FN_MP_SETOBJVAR]          =  "mp_setobjvar",
   [FN_MP_SETPROBTYPE]        =  "mp_setprobtype",
   [FN_NASH_ADDMPBYID]        =  "nash_addmpbyid",
   [FN_OVF_ADDARG]            =  "ovf_addarg",
   [FN_OVF_FINALIZE]          =  "ovf_finalize",
   [FN_OVF_SETRHO]            =  "ovf_setrho",
   [FN_OVF_UPDATEPARAMS]      =  "ovf_updateparams",
   [FN_OVFPARAM_SETDEFAULT]   =  "ovf_setdefault",
   [FN_OVFPARAM_UPDATE]       =  "ovf_param_update",
};

const unsigned empapis_len = sizeof(empapis)/sizeof(empapis[0]);


static VmValue as_mp(void *obj)   { return MPOBJ_VAL(obj);  }
static VmValue as_nash(void *obj) { return NASHOBJ_VAL(obj); }
static VmValue as_ovf(void *obj)  { return OVFOBJ_VAL(obj); }

static unsigned mp_getuid(void *o)   { return mpid2uid(((MathPrgm*)o)->id); }
static unsigned nash_getuid(void *o) { return nashid2uid(((Nash*)o)->id); }

const EmpNewObjCall empnewobjs[] = {
   [FN_CCFLIB_NEW]  = { .fn = ccflib_newobj, .obj2vmval = as_mp,  .argc = 2, .get_uid = mp_getuid},
   [FN_MP_NEW]      = { .fn = mp_newobj,     .obj2vmval = as_mp,  .argc = 2, .get_uid = mp_getuid },
   [FN_NASH_NEW]     = { .fn = nash_newobj,    .obj2vmval = as_nash, .argc = 1, .get_uid = nash_getuid },
   [FN_OVF_NEW]     = { .fn = ovf_newobj,    .obj2vmval = as_ovf, .argc = 1, .get_uid = NULL },
};

const unsigned empnewobjs_len = sizeof(empnewobjs)/sizeof(empnewobjs[0]);

const char * const empnewobjs_names[] = {
   [FN_CCFLIB_NEW]            =  "ccflib_new",
   [FN_MP_NEW]                =  "mp_new",
   [FN_NASH_NEW]              =  "nash_new",
   [FN_OVF_NEW]               =  "ovf_new",
};
