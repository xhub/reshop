// signatures
// docstrings are generated from doxygen thanks to an external tool
// (doxy2swig.py)
//%feature("autodoc", 0);

// named parameters (broken with overloaded function)
// %feature("kwargs");

// Do not create default constructor
%nodefaultctor;


// to avoid name conflicts
%runtime %{
#define SWIG_FILE_WITH_INIT

#ifdef __GNUC__
_Pragma("GCC diagnostic error \"-Wimplicit-function-declaration\"")
_Pragma("GCC diagnostic ignored \"-Wunused-parameter\"")
#define UNUSED __attribute__((unused))

#define RHP_LIKELY(x) (__builtin_expect(!!(x),1))
#define RHP_UNLIKELY(x) (__builtin_expect(!!(x),0))

#elif __clang__

_Pragma("clang diagnostic error \"-Wimplicit-function-declaration\"")
_Pragma("clang diagnostic ignored \"-Wunused-parameter\"")
_Pragma("clang diagnostic ignored \"-Wunused-function\"")
#define UNUSED __attribute__((unused))

#define RHP_LIKELY(x) (__builtin_expect(!!(x),1))
#define RHP_UNLIKELY(x) (__builtin_expect(!!(x),0))


#else

#define UNUSED 
#define RHP_LIKELY(x)    (x)
#define RHP_UNLIKELY(x)  (x)

#endif



/* Includes the header in the wrapper code */
#include "reshop.h"
#include "rhpidx.h"
#include "asprintf.h"

#include "status.h"

// TODO: just for prototyping
#include "pool.h"

SWIGINTERN int errcode_rhp2swig(int code)
{
   switch (code) {
   case Error_DimensionDifferent:         return SWIG_ValueError;
   case Error_DuplicateValue:             return SWIG_ValueError;
//   case Error_EMPIncorrectInput,
   case Error_EMPIncorrectSyntax:         return SWIG_SyntaxError;
   case Error_EMPRuntimeError:            return SWIG_RuntimeError;
//   case Error_EvaluationFailed,
   case Error_FileOpenFailed:             return SWIG_IOError;
   case Error_GamsIncompleteSetupInfo:    return SWIG_SystemError;
//   case Error_GamsCallFailed,
//   case Error_GamsSolverCallFailed,
//   case Error_HDF5_IO,
//   case Error_HDF5_NullPointer,
//   case Error_HDF5_Unsupported,
//   case Error_IncompleteModelMetadata,
//   case Error_Inconsistency,
   case Error_IndexOutOfRange:             return SWIG_IndexError;
   case Error_InsufficientMemory:          return SWIG_MemoryError;
//   case Error_InvalidArgument,
//   case Error_InvalidModel,
//   case Error_InvalidOpCode,
   case Error_InvalidValue:                 return SWIG_ValueError;
//   case Error_MathError,
//   case Error_ModelInfeasible,
//   case Error_ModelUnbounded,
//   case Error_NameTooLongForGams,
//   case Error_NotEmpty,
//   case Error_NotFound,
//   case Error_NotImplemented,
//   case Error_NotInitialized,
//   case Error_NotScalarVariable,
   case Error_NullPointer:                  return SWIG_NullReferenceError;
//   case Error_OperationNotAllowed,
   case Error_OptionNotFound:               return SWIG_ValueError;
//   case Error_OvfMissingParam,
   case Error_RuntimeError:                 return SWIG_RuntimeError;
   case Error_SizeTooSmall:                 return SWIG_ValueError;
   case Error_SizeTooLarge:                 return SWIG_ValueError;
//   case Error_SolverCreateFailed,
//   case Error_SolverError,
   case Error_SolverInvalidName:            return SWIG_ValueError;
//   case Error_SolverModifyProblemFailed,
//   case Error_SymbolicDifferentiation,
//   case Error_SymbolNotInTheGamsRim,
   case Error_SystemError:                   return SWIG_SystemError;
//   case Error_UnExpectedData,
//   case Error_UnsupportedOperation,
//   case Error_WrongModelForFunction,
//   case Error_WrongNumericalValue,
   case Error_WrongOptionValue:              return SWIG_ValueError;
   case Error_WrongParameterValue:           return SWIG_ValueError;
   default: return SWIG_RuntimeError;
   }
}


#define RHP_FAIL(expr, msg) do { int rc42 = (expr); if (RHP_OK != rc42) { SWIG_exception_fail(errcode_rhp2swig(rc42), msg); } } while(0);

#define IO_PRINT_SWIG(expr) { int status42 = (expr); if (RHP_UNLIKELY(status42 < 0)) { \
   SWIG_exception_fail(SWIG_RuntimeError, "write error"); } }

typedef rhp_idx rhp_idx_typed;

#include "python_structs.h"

const BasisStatus BasisUnsetObj      = {.status = RhpBasisUnset};
const BasisStatus BasisLowerObj      = {.status = RhpBasisLower};
const BasisStatus BasisUpperObj      = {.status = RhpBasisUpper};
const BasisStatus BasisBasicObj      = {.status = RhpBasisBasic};
const BasisStatus BasisSuperBasicObj = {.status = RhpBasisSuperBasic};
const BasisStatus BasisFixedObj      = {.status = RhpBasisFixed};
const BasisStatus BasisInvalidObj    = {.status = INT8_MAX};

const RhpBackendType BackendGamsGmoObj  = {.backend = RhpBackendGamsGmo};
const RhpBackendType BackendReSHOPObj   = {.backend = RhpBackendReSHOP};
const RhpBackendType BackendJuliaObj    = {.backend = RhpBackendJulia};
const RhpBackendType BackendAmplObj     = {.backend = RhpBackendAmpl};
const RhpBackendType BackendInvalidObj  = {.backend = INT8_MAX};


#define CHK_RET(expr) { \
  int status42 = (expr); \
  if (status42 != RHP_OK) { \
    const char *msg = rhp_status_descr(status42); \
    SWIG_exception(SWIG_RuntimeError, msg); \
 } \
}

#define CHK_RET_EXCEPTION(expr) { \
  int status42 = (expr); \
  if (status42 != RHP_OK) { \
    exception_code = status42; \
    exception_msg = rhp_status_descr(status42); \
 } \
}

#ifndef MY_CONCAT
#define MY_CONCAT(x, y) x ## _ ## y
#endif

#define MAX(a, b) (a) >= (b) ? (a) : (b)

#include "tlsdef.h"

static tlsvar PyObject* reshop_mod = NULL;
// For Exception-stylen handling
static tlsvar const char *exception_msg = NULL;
static tlsvar int exception_code = RHP_OK;

static PyObject * basisstatus_getobj(int bstat)
{

   if (!reshop_mod) {
      PyObject* sys_mod_dict = PyImport_GetModuleDict();
      /* get the reshop module object */ 
      PyObject* reshop_mod = PyMapping_GetItemString(sys_mod_dict, (char *)"reshop");
 
      if (!reshop_mod) { 
        PyErr_SetString(PyExc_RuntimeError, "Couldn't find module 'reshop'");
        return NULL;
      }
   }

   PyObject *o;

   switch (bstat) {
   case  RhpBasisUnset:       o = PyObject_GetAttrString(reshop_mod, "BasisUnset");
   case  RhpBasisLower:       o = PyObject_GetAttrString(reshop_mod, "BasisLower");
   case  RhpBasisUpper:       o = PyObject_GetAttrString(reshop_mod, "BasisUpper");
   case  RhpBasisBasic:       o = PyObject_GetAttrString(reshop_mod, "BasisBasic");
   case  RhpBasisSuperBasic:  o = PyObject_GetAttrString(reshop_mod, "BasisSuperBasic");
   case  RhpBasisFixed:       o = PyObject_GetAttrString(reshop_mod, "BasisFixed");
   default:                   o = PyObject_GetAttrString(reshop_mod, "BasisInvalid");
   }

   return o;
}


static PyObject * backendtype_getobj(int backendtype)
{

   if (!reshop_mod) {
      PyObject* sys_mod_dict = PyImport_GetModuleDict();
      /* get the reshop module object */ 
      PyObject* reshop_mod = PyMapping_GetItemString(sys_mod_dict, (char *)"reshop");
 
      if (!reshop_mod) { 
        PyErr_SetString(PyExc_RuntimeError, "Couldn't find module 'reshop'");
        return NULL;
      }
   }

   PyObject *o;

   switch (backendtype) {
   case  RhpBackendGamsGmo:  o = PyObject_GetAttrString(reshop_mod, "BackendGamsGmo");
   case  RhpBackendReSHOP:   o = PyObject_GetAttrString(reshop_mod, "BackendReSHOP");
   case  RhpBackendJulia:    o = PyObject_GetAttrString(reshop_mod, "BackendJulia");
   case  RhpBackendAmpl:     o = PyObject_GetAttrString(reshop_mod, "BackendAmpl");
   default:                  o = PyObject_GetAttrString(reshop_mod, "BackendInvalid");
   }

   return o;
}
%}

%include target_datatypes.i

#ifdef SWIGPYTHON
// numpy macros
%include defines_python.i
%include numpy.i

%init %{
  import_array();
%}

#endif /* SWIGPYTHON */


#ifdef SWIGMATLAB
%include numpy_matlab.i
#endif /* SWIGMATLAB */
