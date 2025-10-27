#include "empinterp_vm_utils.h"
#include "empinterp_utils.h"
#include "gamsapi_utils.h"
#include "printout.h"

#include "gmdcc.h"

VmValue vmvals_getnil(void)
{
   return NULL_VAL;
}

const char* vmval_typename(VmValue vmval)
{
   if (IS_FINITE_DBL(vmval)) { return "finite double"; }

   uint64_t sig = vmval & MASK_SIGNATURE;

   switch (sig) {
   case SIGNATURE_QNAN:   return "qNaN";
   case SIGNATURE_VALUES:   return "value";
   case SIGNATURE_INTEGER:   return "integer";
   case SIGNATURE_UINTEGER:   return "unsigned integer";
   case SIGNATURE_LOOPVAR:   return "loop variable";
   case SIGNATURE_POINTER:   return "generic pointer";
   case SIGNATURE_STRING:   return "packed string";
   case SIGNATURE_MPOBJ:     return "MP object";
   case SIGNATURE_NASHOBJ:    return "Nash object";
   case SIGNATURE_OVFOBJ:    return "OVF object";
   case SIGNATURE_GMSSYMITER:    return "GmsSymIterator";
   case SIGNATURE_REGENTRY:    return "Register entry";
   case SIGNATURE_ARCOBJ:    return "Arc object";
   default: return "ERROR invalid vmval type";
   }

}


int vmval_is_regentry(VmValueArray * vmvals, unsigned idx)
{
   if (idx >= vmvals->len) {
      error("[empcompiler] ERROR: Label object index %u is not in [0,%u)\n",
            idx, vmvals->len);
      return Error_EMPRuntimeError;
   }

   VmValue v = vmvals->arr[idx];
   if (!IS_REGENTRY(v)) {
      error("[empcompiler] ERROR: global object at index %u is not a Label, "
            "rather it has type %s\n", idx, vmval_typename(v));
      return Error_EMPRuntimeError;

   }

   return OK;
}

int vmval_is_gmssymiter(VmValueArray * vmvals, unsigned idx)
{
   if (idx >= vmvals->len) {
      error("[empcompiler] ERROR: Label object index %u is not in [0,%u)\n",
            idx, vmvals->len);
      return Error_EMPRuntimeError;
   }

   VmValue v = vmvals->arr[idx];
   if (!IS_GMSSYMITER(v)) {
      error("[empcompiler] ERROR: global object at index %u is not a GmsFilter, "
            "rather it has type %s\n", idx, vmval_typename(v));
      return Error_EMPRuntimeError;

   }

   return OK;
}

int vmval_is_arcobj(VmValueArray * vmvals, unsigned idx)
{
   if (idx >= vmvals->len) {
      error("[empcompiler] ERROR: Arc object index %u is not in [0,%u)\n",
            idx, vmvals->len);
      return Error_EMPRuntimeError;
   }

   VmValue v = vmvals->arr[idx];
   if (!IS_ARCOBJ(v)) {
      error("[empcompiler] ERROR: global object at index %u is not an ArcObj, "
            "rather it has type %s\n", idx, vmval_typename(v));
      return Error_EMPRuntimeError;

   }

   return OK;
}

int error_ident_origin_dct(const IdentData * restrict ident, const char* fn)
{
   const Lexeme *lexeme = &ident->lexeme;
   error("[empinterp] ERROR on line %u: function %s() was called for ident '%.*s' resolved in the DCT."
         "Please report this bug.\n", lexeme->linenr, fn, lexeme->len, lexeme->start);
   return Error_EMPRuntimeError;
}

int vm_store_set_nrecs_gdx(Interpreter * restrict interp, EmpVm * restrict vm,
                           const IdentData * restrict ident, GIDX_TYPE *gidx)
{
   IntArray loopset = namedints_at(&interp->globals.sets, ident->idx);
   assert(valid_set(loopset));

   S_CHECK(rhp_uint_add(&vm->uints, loopset.len));
   *gidx = vm->uints.len-1;

   return OK;
}

int vm_store_set_nrecs_gmd(Interpreter * restrict interp, EmpVm * restrict vm,
                           const IdentData * restrict ident, GIDX_TYPE *gidx)
{
   assert(interp->gmd);
   int nrecs;
   GMD_CHK(gmdSymbolInfo, interp->gmd, ident->ptr, GMD_NRRECORDS, &nrecs, NULL, NULL);
   if (nrecs < 0) {
      return runtime_error(interp->linenr);
   }
   S_CHECK(rhp_uint_add(&vm->uints, (unsigned)nrecs));
   *gidx = vm->uints.len-1;

   return OK;
}

int vmdata_consume_scalarvar(VmData *data, rhp_idx *vi)
{
   unsigned vlen = data->equvar.v.size;

   if (vlen > 1) {
      error("[empvm] ERROR: expecting a variable of size at most 1, got %u\n", vlen);
      return Error_EMPIncorrectInput;
   }

   *vi = vlen == 0 ? IdxNA : avar_fget(&data->equvar.v, 0);

   avar_reset(&data->equvar.v);

   return OK;
}


