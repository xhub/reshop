#include "checks.h"
#include "container.h"
#include "container_ops.h"
#include "equvar_helpers.h"
#include "macros.h"
#include "mdl.h"
#include "mdl_data.h"
#include "mdl_ops.h"
#include "mdl_rhp.h"
#include "reshop.h"

int rhp_mdl_setsolvername(Model *mdl, const char *solvername)
{ 
   S_CHECK(chk_mdl(mdl, __func__));
   S_CHECK(chk_arg_nonnull(solvername, 2, __func__));
      
   return mdl_setsolvername(mdl, solvername);
}

/**
 * @brief Get the solver name
 *
 * @group publicAPI
 *
 * @param      mdl         the model
 * @param[out] solvername  the pointer to the string
 *
 * @return                 the name of the solver
 */
int rhp_mdl_getsolvername(const Model *mdl, char const ** solvername)
{
   S_CHECK(chk_mdl(mdl, __func__));
   S_CHECK(chk_arg_nonnull(solvername, 2, __func__));
      
   return mdl->ops->getsolvername(mdl, solvername);
}

int rhp_mdl_gettype(const Model *mdl, unsigned *type)
{
   S_CHECK(chk_mdl(mdl, __func__));
   S_CHECK(chk_arg_nonnull(type, 2, __func__));

   ModelType type_;
   S_CHECK(mdl_gettype(mdl, &type_));
   *type = type_;

   return OK;
}

int rhp_mdl_getsense(const Model *mdl, unsigned *objsense)
{
   S_CHECK(chk_mdl(mdl, __func__));
   S_CHECK(chk_arg_nonnull(objsense, 2, __func__));

   RhpSense objsense_;
   S_CHECK(mdl->ops->getsense(mdl, &objsense_));
   *objsense = objsense_;
   return OK;
}

int rhp_mdl_settype(Model *mdl, unsigned type)
{
   S_CHECK(chk_mdl(mdl, __func__));
   if (!valid_mdltype(type)) {
      error("%s ERROR: invalid model type value %u\n", __func__, type);
      return Error_InvalidValue;
   }

   return mdl_settype(mdl, type);
}

int rhp_mdl_setobjsense(Model *mdl, unsigned objsense)
{
   S_CHECK(chk_mdl(mdl, __func__));
   if (!valid_sense(objsense)) {
      error("%s ERROR: invalid objsense value %u\n", __func__, objsense);
      return Error_InvalidValue;
   }

   return mdl->ops->setsense(mdl, objsense);
}

int rhp_mdl_resize(Model *mdl, unsigned n, unsigned m)
{
   S_CHECK(chk_mdl(mdl, __func__));
   return ctr_resize(&mdl->ctr, n, m);
}

const char *rhp_mdl_printvarname(const Model *mdl, rhp_idx vi)
{
   SN_CHECK(chk_mdl(mdl, __func__));
   return ctr_printvarname(&mdl->ctr, vi);
}

const char* rhp_mdl_printequname(const Model *mdl, rhp_idx ei)
{
   SN_CHECK(chk_mdl(mdl, __func__));
   return ctr_printequname(&mdl->ctr, ei);
}

MathPrgm* rhp_mdl_getmpforvar(const Model *mdl, rhp_idx vi)
{
   SN_CHECK(chk_mdl(mdl, __func__));
   return mdl_getmpforvar(mdl, vi);
}

MathPrgm* rhp_mdl_getmpforequ(const Model *mdl, rhp_idx ei)
{
   SN_CHECK(chk_mdl(mdl, __func__));
   return mdl_getmpforequ(mdl, ei);
}

int rhp_mdl_getequtype(Model *mdl, rhp_idx ei, unsigned *type, unsigned *cone)
{
   S_CHECK(chk_rmdl(mdl, __func__));
   S_CHECK(chk_arg_nonnull(type, 3, __func__));
   S_CHECK(chk_arg_nonnull(cone, 4, __func__));

   return ctr_getequtype(&mdl->ctr, ei, type, cone);
}

int rhp_mdl_getspecialfloats(const Model *mdl, double *minf, double *pinf, double* nan)
{
   S_CHECK(chk_rmdl(mdl, __func__));
   S_CHECK(chk_arg_nonnull(minf, 2, __func__));
   S_CHECK(chk_arg_nonnull(pinf, 3, __func__));
   S_CHECK(chk_arg_nonnull(nan, 4, __func__));

   return ctr_getspecialfloats(&mdl->ctr, minf, pinf, nan);
}

int rhp_mdl_getequbyname(const Model *mdl, const char *name, rhp_idx *ei)
{
   S_CHECK(chk_rmdl(mdl, __func__));
   S_CHECK(chk_arg_nonnull(name, 2, __func__));
   S_CHECK(chk_arg_nonnull(ei, 3, __func__));

   return mdl->ctr.ops->getequbyname(&mdl->ctr, name, ei);
}

int rhp_mdl_getvarbyname(const Model *mdl, const char *name, rhp_idx *vi)
{
   S_CHECK(chk_rmdl(mdl, __func__));
   S_CHECK(chk_arg_nonnull(name, 2, __func__));
   S_CHECK(chk_arg_nonnull(vi, 3, __func__));

   return mdl->ctr.ops->getvarbyname(&mdl->ctr, name, vi);
}

int rhp_mdl_exportmodel(struct rhp_mdl *mdl, struct rhp_mdl *mdl_dst)
{
   S_CHECK(chk_rmdl(mdl, __func__));
   S_CHECK(chk_rmdl(mdl_dst, __func__));

   return mdl_export(mdl, mdl_dst);
}



#define WRAP_VAR_GETTER(name, argtype, argname) \
int rhp_mdl_ ## name (const Model *mdl, rhp_idx vi, argtype argname) { \
   S_CHECK(chk_mdl(mdl, __func__)); \
   S_CHECK(vi_inbounds(vi, ctr_nvars_total(&mdl->ctr), __func__)); \
   S_CHECK(chk_arg_nonnull(argname, 3, __func__)); \
   return mdl->ctr.ops->name (&mdl->ctr, vi, argname); \
}

WRAP_VAR_GETTER(getvarperp,rhp_idx*,ei)
WRAP_VAR_GETTER(getvarbasis,rhp_idx*,basis)
WRAP_VAR_GETTER(getvarlb,double*,lower_bound)
WRAP_VAR_GETTER(getvarmult,double*,mult)
WRAP_VAR_GETTER(getvarval,double*,value)
WRAP_VAR_GETTER(getvartype,unsigned*,type)
WRAP_VAR_GETTER(getvarub,double*,upper_bound)

#define WRAP_GETTER2(argidx, name, argtype1, argname1, argtype2, argname2) \
int rhp_mdl_ ## name (const Model *mdl, rhp_idx argidx, argtype1 argname1, argtype2 argname2) { \
   S_CHECK(chk_mdl(mdl, __func__)); \
   S_CHECK(chk_arg_nonnull(argname1, 3, __func__)); \
   S_CHECK(chk_arg_nonnull(argname2, 4, __func__)); \
   assert(mdl->ctr.ops->name); \
   return mdl->ctr.ops->name (&mdl->ctr, argidx, argname1, argname2); \
}

#define WRAP_VAR_GETTER2(...) WRAP_GETTER2(vi, __VA_ARGS__)

WRAP_VAR_GETTER2(getvarbounds,double*,lb,double*,ub)


#define WRAP_VARS_GETTER(name, argtype, argname) \
int rhp_mdl_ ## name (const Model *mdl, Avar *v, argtype argname) { \
   S_CHECK(chk_mdl(mdl, __func__)); \
   S_CHECK(chk_arg_nonnull(v, 2, __func__)); \
   S_CHECK(chk_arg_nonnull(argname, 3, __func__)); \
   return mdl->ctr.ops->name (&mdl->ctr, v, argname); \
}

WRAP_VARS_GETTER(getvarsmult,double*,mult)
WRAP_VARS_GETTER(getvarsval,double*,values)
WRAP_VARS_GETTER(getvarsbasis,int*,bstat)


#define WRAP_EQU_GETTER(name, argtype, argname) \
int rhp_mdl_ ## name (const Model *mdl, rhp_idx ei, argtype argname) { \
   S_CHECK(chk_mdl(mdl, __func__)); \
   S_CHECK(ei_inbounds(ei, ctr_nequs_total(&mdl->ctr), __func__)); \
   S_CHECK(chk_arg_nonnull(argname, 3, __func__)); \
   return mdl->ctr.ops->name (&mdl->ctr, ei, argname); \
}

WRAP_EQU_GETTER(getequcst,double*,cst)
WRAP_EQU_GETTER(getequperp,rhp_idx*,match)
WRAP_EQU_GETTER(getequbasis,rhp_idx*,bstat)
WRAP_EQU_GETTER(getequval,double*,level)
WRAP_EQU_GETTER(getequmult,double*,mult)

#define WRAP_EQUS_GETTER(name, argtype, argname) \
int rhp_mdl_ ## name (const Model *mdl, Aequ *e, argtype argname) { \
   S_CHECK(chk_mdl(mdl, __func__)); \
   S_CHECK(chk_arg_nonnull(e, 2, __func__)); \
   S_CHECK(chk_arg_nonnull(argname, 3, __func__)); \
   return mdl->ctr.ops->name (&mdl->ctr, e, argname); \
}

WRAP_EQUS_GETTER(getequsmult,double*,mult)
WRAP_EQUS_GETTER(getequsval,double*,values)
WRAP_EQUS_GETTER(getequsbasis,int*,bstat)

#define WRAP_SINGLE_SETTER(name, argtype, argname) \
int rhp_mdl_set ## name (Model *mdl, argtype argname) { \
   S_CHECK(chk_mdl(mdl, __func__)); \
   assert(mdl->ops->set ## name); \
   return mdl->ops->set ## name (mdl, argname); \
}

WRAP_SINGLE_SETTER(objvar, rhp_idx, vi)
//WRAP_SINGLE_SETTER(objequ, rhp_idx, ei)

#define ei_maxval(ctr) ctr_nequs_total(ctr)
#define vi_maxval(ctr) ctr_nvars_total(ctr)

#define WRAP_SETTER(argidx, name, argtype, argname) \
int rhp_mdl_ ## name (Model *mdl, rhp_idx argidx, argtype argname) { \
   S_CHECK(chk_mdl(mdl, __func__)); \
   S_CHECK(argidx ## _inbounds(argidx, argidx ## _maxval(&mdl->ctr), __func__)); \
   assert(mdl->ctr.ops->name); \
   return mdl->ctr.ops->name (&mdl->ctr, argidx, argname); \
}

#define WRAP_SETTER2(argidx, name, argtype1, argname1, argtype2, argname2) \
int rhp_mdl_ ## name (Model *mdl, rhp_idx argidx, argtype1 argname1, argtype2 argname2) { \
   S_CHECK(chk_mdl(mdl, __func__)); \
   S_CHECK(argidx ## _inbounds(argidx, argidx ## _maxval(&mdl->ctr), __func__)); \
   assert(mdl->ctr.ops->name); \
   return mdl->ctr.ops->name (&mdl->ctr, argidx, argname1, argname2); \
}

#define WRAP_VAR_SETTER(...) WRAP_SETTER(vi, __VA_ARGS__)
#define WRAP_VAR_SETTER2(...) WRAP_SETTER2(vi, __VA_ARGS__)
#define WRAP_EQU_SETTER(...) WRAP_SETTER(ei, __VA_ARGS__)
#define WRAP_EQU_SETTER2(...) WRAP_SETTER2(ei, __VA_ARGS__)

//WRAP_VAR_SETTER(setvarperp,rhp_idx*,match)
//WRAP_VAR_SETTER(setvarstatone,unsigned,bstat)
WRAP_EQU_SETTER(setvarbasis,int,basis_status)
WRAP_VAR_SETTER2(setvarbounds,double,lb,double,ub)
WRAP_VAR_SETTER(setvarlb,double,lb)
WRAP_VAR_SETTER(setvarmult,double,mult)
WRAP_VAR_SETTER(setvarname,const char*,name)
WRAP_VAR_SETTER(setvartype,unsigned,type)
WRAP_VAR_SETTER(setvarub,double,ub)
WRAP_VAR_SETTER(setvarval,double,value)

WRAP_EQU_SETTER(setequbasis,int,basis_status)
WRAP_EQU_SETTER(setequcst,double,cst)
WRAP_EQU_SETTER(setequname,const char*,name)
WRAP_EQU_SETTER(setequmult,double,mult)
WRAP_EQU_SETTER(setequval,double,value)
WRAP_EQU_SETTER(setequvarperp,rhp_idx,vi)
WRAP_EQU_SETTER2(setequtype,unsigned,type,unsigned,cone)


#define WRAP_MDL_FN1(qualifier,name, argtype1, argname1) \
int rhp_mdl_ ## name (qualifier Model *mdl, argtype1 argname1) { \
   S_CHECK(chk_mdl(mdl, __func__)); \
   return mdl_ ## name (mdl, argname1); \
}

#define WRAP_MDL_FN2(qualifier, name, argtype1, argname1, argtype2, argname2) \
int rhp_mdl_ ## name (qualifier Model *mdl, argtype1 argname1, argtype2 argname2) { \
   S_CHECK(chk_mdl(mdl, __func__)); \
   return mdl_ ## name (mdl, argname1, argname2); \
}

#define WRAP_CTR_FN1(qualifier,name, argtype1, argname1) \
int rhp_mdl_ ## name (qualifier Model *mdl, argtype1 argname1) { \
   S_CHECK(chk_mdl(mdl, __func__)); \
   return ctr_ ## name (&mdl->ctr, argname1); \
}

#define WRAP_CTR_FN2(qualifier, name, argtype1, argname1, argtype2, argname2) \
int rhp_mdl_ ## name (qualifier Model *mdl, argtype1 argname1, argtype2 argname2) { \
   S_CHECK(chk_mdl(mdl, __func__)); \
   return ctr_ ## name (&mdl->ctr, argname1, argname2); \
}
#define WRAP_MDL_GETTER1(...)  WRAP_MDL_FN1(const, __VA_ARGS__)
#define WRAP_MDL_SETTER1(...)  WRAP_MDL_FN1(/**/, __VA_ARGS__)
#define WRAP_CTR_GETTER1(...)  WRAP_CTR_FN1(const, __VA_ARGS__)
#define WRAP_CTR_GETTER2(...)  WRAP_CTR_FN2(const, __VA_ARGS__)
#define WRAP_CTR_SETTER2(...)  WRAP_CTR_FN2(/**/, __VA_ARGS__)


WRAP_MDL_GETTER1(getobjequs,Aequ*,objs)
WRAP_MDL_GETTER1(getobjequ,rhp_idx*,objequ)
WRAP_MDL_GETTER1(getobjvar,rhp_idx*,objvar)
WRAP_MDL_GETTER1(getmodelstat,int*,modelstat)
WRAP_MDL_GETTER1(getsolvestat,int*,solvestat)


WRAP_CTR_GETTER1(getallequsmult,double*,mult)
WRAP_CTR_GETTER1(getallequsval,double*,vals)
WRAP_CTR_GETTER1(getallvarsmult,double*,mult)
WRAP_CTR_GETTER1(getallvarsval,double*,vals)

WRAP_CTR_SETTER2(setequrhs,rhp_idx,ei,double,val)

#define WRAP_MDL_QUERY2(name, rettype, errval, arg1type, arg1) \
rettype rhp_mdl_ ## name (const Model *mdl, arg1type arg1) { \
   if (chk_mdl(mdl, __func__) != OK) { return errval; }; \
   return mdl_ ## name (mdl, arg1); \
}
WRAP_MDL_QUERY2(modelstattxt,const char*,NULL,int,modelstat)
WRAP_MDL_QUERY2(solvestattxt,const char*,NULL,int,solvestat)

#define WRAP_CTR_QUERY(name, rettype, errval) \
rettype rhp_mdl_ ## name (const Model *mdl) { \
   if (chk_mdl(mdl, __func__) != OK) { return errval; }; \
   return ctr_ ## name (&mdl->ctr); \
}
WRAP_CTR_QUERY(nequs, unsigned, UINT_MAX)
WRAP_CTR_QUERY(nvars, unsigned, UINT_MAX)
WRAP_CTR_QUERY(nequs_total, unsigned, UINT_MAX)
WRAP_CTR_QUERY(nvars_total, unsigned, UINT_MAX)
