#include "empinterp_vm_utils.h"
#include "printout.h"

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
   case SIGNATURE_MPEOBJ:    return "MPE object";
   case SIGNATURE_OVFOBJ:    return "OVF object";
   case SIGNATURE_GMSSYMITER:    return "GmsSymIterator";
   case SIGNATURE_REGENTRY:    return "Register entry";
   case SIGNATURE_ARCOBJ:    return "Edge object";
   }

   return "ERROR invalid vmval type";
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
