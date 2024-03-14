#include "container.h"
#include "equvar_metadata.h"
#include "macros.h"
#include "mdl.h"
#include "printout.h"

struct equ_meta *equmeta_alloc(void)
{
   struct equ_meta *emd;

   MALLOC_NULL(emd, struct equ_meta, 1);
   equmeta_init(emd);

   return emd;
}

void equmeta_dealloc(struct equ_meta **emd)
{
   if (emd && (*emd)) {
      FREE((*emd));
      (*emd) = NULL;
   }
}

void equmeta_init(struct equ_meta *emd)
{
   emd->role = EquUndefined;
   emd->ppty = EquPptyNone;
   emd->dual = IdxNA;
   emd->mp_id = MpId_NA;
}

/* TODO(xhub) DEL? */
VarMeta *varmeta_alloc(void)
{
   VarMeta *vmd;

   MALLOC_NULL(vmd, VarMeta, 1);
   varmeta_init(vmd);

   return vmd;
}

void varmeta_dealloc(VarMeta **vmd)
{
   if (vmd && (*vmd)) {
      FREE((*vmd));
      (*vmd) = NULL;
   }
}

void varmeta_init(VarMeta *vmd)
{
   vmd->type = VarUndefined;
   vmd->ppty = VarPptyNone;
   vmd->dual = IdxNA;
   vmd->mp_id = MpId_NA;
}

const char* equrole_name(EquRole role)
{
   switch (role) {
   case EquUndefined:
      return "undefined";
   case EquObjective:
      return "objective";
   case EquConstraint:
      return "constraint";
   case EquViFunction:
      return "VI function";
   default:
      return "INVALID";
   }
}

const char* varrole_name(VarRole type)
{
   switch (type) {
   case VarUndefined:
      return "undefined variable";
   case VarObjective:
      return "objective variable";
   case VarPrimal:
      return "primal variable";
   case VarDual:
      return "dual variable";
   default:
      return "INVALID";
   }
}

static const char* varbasicppty_name(VarPptyType type)
{
   switch (type) {
   case VarPptyNone:                return "undefined";
   case VarIsObjMin:                return "minimize objective variable";
   case VarIsObjMax:                return "maximize objective variable";
   case VarIsDualVar:               return "dual variable w.r.t. a constraint";
   case VarIsExplicitlyDefined:     return "explicitly defined variable";
   case VarIsImplicitlyDefined:     return "implicitly defined variable";
   case VarPerpToViFunction:        return "matched variable";
   case VarPerpToZeroFunctionVi:    return "matched variable (to a zero function)";
   default:                         return "ERROR INVALID BASIC";
   }
}

static const char* varextendedppty_name(VarPptyType type)
{
   switch (type) {
   case VarIsSolutionVar:           return "Solution variable";
   case VarIsShared:                return "Shared variable";
   case VarIsDeleted:               return "Deleted variable";
   default:                         return "ERROR INVALID EXTENDED";
   }
}

void varmeta_print(const VarMeta * restrict vmeta, rhp_idx vi, const Model *mdl,
                   unsigned mode, unsigned offset)
{
   printout(mode, "%*sVariable '%s' has type %s\n", offset, "",
            mdl_printvarname(mdl, vi), varrole_name(vmeta->type));

   VarPptyType ppty = vmeta->ppty;
   printout(mode, "%*sVariable '%s' has properties: %s", offset, "",
            mdl_printvarname(mdl, vi), varbasicppty_name(ppty));

   VarPptyType ext_types[] = {VarIsSolutionVar, VarIsDeleted};
   for (unsigned i = 0, len = ARRAY_SIZE(ext_types); i < len; ++i) {
      if (ppty & ext_types[i]) {
         printstr(mode, ", "); printstr(mode, varextendedppty_name(ext_types[i]));
      }
   }
   printstr(mode, "\n");


   if (valid_ei(vmeta->dual)) {
      printout(mode, "%*sVariable '%s' is dual to equation '%s'\n", offset, "",
               mdl_printvarname(mdl, vi), mdl_printequname(mdl, vmeta->dual));
   }
}

void equmeta_rolemismatchmsg(const Container *ctr, rhp_idx ei, EquRole actual,
                             EquRole expected, const char *fn)
{
   error("%s :: ERROR: the equation %s has type %s. It should be %s\n", fn,
         ctr_printequname(ctr, ei), equrole_name(actual),
         equrole_name(expected));
}
