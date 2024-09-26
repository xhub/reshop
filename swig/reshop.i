%module reshop

%{
/* Includes the header in the wrapper code */
#include "reshop.h"
#include "rhpidx.h"
#include "asprintf.h"

// TODO: just for prototyping
#include "pool.h"
/*
#ifdef __GNUC__

#define RHP_LIKELY(x) (__builtin_expect(!!(x),1))
#define RHP_UNLIKELY(x) (__builtin_expect(!!(x),0))

#else

#define RHP_LIKELY(x)    (x)
#define RHP_UNLIKELY(x)  (x)

#endif
*/


#define RHP_FAIL(expr, msg) do { if (RHP_OK != (expr)) { SWIG_exception_fail(SWIG_RuntimeError, msg); } } while(0);
#define IO_CALL_SWIG(expr) { int status42 = (expr); if (RHP_UNLIKELY(status42 < 0)) { \
   SWIG_exception_fail(SWIG_RuntimeError, "write error"); } }

typedef rhp_idx rhp_idx_typed;

#include "python_structs.h"

const BasisStatus BasisUnsetObj      = {.status = RHP_BASIS_UNSET};
const BasisStatus BasisLowerObj      = {.status = RHP_BASIS_LOWER};
const BasisStatus BasisUpperObj      = {.status = RHP_BASIS_UPPER};
const BasisStatus BasisBasicObj      = {.status = RHP_BASIS_BASIC};
const BasisStatus BasisSuperBasicObj = {.status = RHP_BASIS_SUPERBASIC};
const BasisStatus BasisFixedObj      = {.status = RHP_BASIS_FIXED};

#define CHK_RET(expr) { \
  int status42 = (expr); \
  if (status42 != RHP_OK) { \
    const char *msg = rhp_status_descr(status42); \
    SWIG_exception(SWIG_RuntimeError, msg); \
 } \
}

#ifndef MY_CONCAT
#define MY_CONCAT(x, y) x ## _ ## y
#endif

#define MAX(a, b) (a) >= (b) ? (a) : (b)

%}

%include start.i

%include linalg_common.i
%include matrix.i

%newobject rhp_aequ_new;
%typemap(newfree) struct rhp_aequ* rhp_aequ_free;
%delobject rhp_aequ_free;

%newobject rhp_avar_new;
%newobject rhp_avar_newcompact;
%newobject rhp_avar_newlist;
%typemap(newfree) struct rhp_avar* rhp_avar_free;
%delobject rhp_avar_free;

%newobject rhp_mdl_new;
%typemap(newfree) struct rhp_mdl* rhp_mdl_free;
%delobject rhp_mdl_new;

#define rhp_idx int

/* We don't need the rhp name */
/* %rename("%(command:sed -e 's/model_//' -e 's/rhp_//' -e 's/reshop_//' -e 's/ctx_/mdl_/' <<<)s") ""; */
%rename("%(strip:[rhp_])s") "";
/* %rename("RHP_CON_GT","") */
%rename(equ_addquad)       rhp_equ_addquadrelative;
%rename(equ_addquad)       rhp_equ_addquadabsolute;

%rename(Model)             rhp_mdl;
%rename(Avar)              rhp_avar;
%rename(Aequ)              rhp_aequ;
%rename(MathPrgm)          rhp_mathprgm;
%rename(NashEquilibrium)   rhp_nash_equilibrium;
%rename(VariableRef)       rhp_variable_ref;
%rename(EquationRef)       rhp_equation_ref;


/* numinputs=0 make the argument disappear from the target language */
%define %abstract_types(init_arg, Type)
 %typemap(in,numinputs=0,noblock=1)
   Type *vout ($*1_ltype temp), 
   Type &vout ($*1_ltype temp) {
   init_arg(temp);
   $1 = &temp;
 }
 %typemap(argout,noblock=1) Type *OUTPUT, Type &OUTPUT {
    int new_flags = (SWIG_POINTER_OWN | %newpointer_flags);
    %append_output(SWIG_NewPointerObj((void*)($1), $1_descriptor, new_flags));
 }
%enddef

/* TODO: what do we want to achieve here? */
abstract_types(rhp_avar_new, struct rhp_avar);
abstract_types(rhp_aequ_new, struct rhp_aequ);

%apply double * OUTPUT { double *val };
%apply double * OUTPUT { double *mult };
%apply int *    OUTPUT { int *bstat };
%apply int *    OUTPUT { int *ei };
%apply int *    OUTPUT { int *vi };
/* This is for get the modelstat and solvestat */
%apply int *    OUTPUT { int *modelstat, int *solvestat };


%apply double *INPUT { double *lb, double *ub };

%apply double *rhp_dblin { const double *coeffs };


%apply (struct rhp_avar *v, double * OUTPUT) { (struct rhp_avar *v, double *vals) };
%apply (struct rhp_avar *v, double * OUTPUT) { (struct rhp_avar *v, double *mult) };
%apply (struct rhp_aequ *e, double * OUTPUT) { (struct rhp_aequ *e, double *vals) };
%apply (struct rhp_aequ *e, double * OUTPUT) { (struct rhp_aequ *e, double *mult) };

%apply ( int DIM1, double* IN_ARRAY1 ) { (unsigned n_vals, double *vals) };
%apply double IN_ARRAY1[ANY] { double *lbs, double *ubs };

%apply (size_t nnz, unsigned *i, unsigned *j, double *x ) { (size_t nnz, rhp_idx *i, rhp_idx *j, double *x) };

%typemap(in,numinputs=0,noblock=1)
   struct rhp_avar *vout ($1_ltype temp), 
   struct rhp_avar &vout ($1_ltype temp) {
   temp = rhp_avar_new();
   $1 = temp;
}
%typemap(argout,noblock=1) struct rhp_avar *vout, struct rhp_avar &vout {
    int new_flags = (SWIG_POINTER_OWN | %newpointer_flags);
    %append_output(SWIG_NewPointerObj((void*)($1), $1_descriptor, new_flags));
}

%typemap(in,numinputs=0,noblock=1)
   struct rhp_aequ *eout ($1_ltype temp), 
   struct rhp_aequ &eout ($1_ltype temp) {
   temp = rhp_aequ_new();
   $1 = temp;
}
%typemap(argout,noblock=1) struct rhp_aequ *eout, struct rhp_aequ &eout {
    int new_flags = (SWIG_POINTER_OWN | %newpointer_flags);
    %append_output(SWIG_NewPointerObj((void*)($1), $1_descriptor, new_flags));
}

%typemap(in,numinputs=0,noblock=1) struct rhp_ovfdef **ovf_def ($*1_ltype temp) {
  $1 = &temp;
}
%typemap(argout,noblock=1) struct rhp_ovfdef **ovf_def {
/* This is owned by reshop */
  int new_flags = 0;
  %append_output(SWIG_NewPointerObj((void*)(*$1), $*1_descriptor, new_flags));
}

%typemap(in, fragment="NumPy_Fragments")
  (double* rhp_array)
  (PyArrayObject* array=NULL, int is_new_object=0)
{
  npy_intp size[1] = { -1 };
  array = obj_to_array_contiguous_allow_conversion($input,
                                                   NPY_DOUBLE,
                                                   &is_new_object);
  if (!array || !require_dimensions(array, 1) ||
      !require_size(array, size, 1)) SWIG_fail;
  $1 = ($1_ltype) array_data(array);
}

%apply double* rhp_array { double *lbs, double *ubs };

/* See http://www.swig.org/Doc4.0/SWIGDocumentation.html#Python_nn44 for more options */
%include "exception.i"
%typemap(out) int {
  if ($1 != RHP_OK) {
    const char *msg = rhp_status_descr($1);
    SWIG_exception(SWIG_RuntimeError, msg);
  }
  if (!$result) {
     $result = VOID_Object;
  }
}

%typemap(out) rhp_idx_typed {
  if (!valid_idx($1)) {
    const char *msg = rhp_status_descr($1);
    SWIG_exception(SWIG_RuntimeError, msg);
  }
  $result = SWIG_From_int($1);
}


/* Parse the header file to generate wrappers */
%include "reshop.h"

%immutable;
%include "python_structs.h"

%constant struct BasisStatus BasisUnset = BasisUnsetObj;
%constant struct BasisStatus BasisLower = BasisLowerObj;
%constant struct BasisStatus BasisUpper = BasisUpperObj;
%constant struct BasisStatus BasisBasic = BasisBasicObj;
%constant struct BasisStatus BasisSuperBasic = BasisSuperBasicObj;
%constant struct BasisStatus BasisFixed = BasisFixedObj;

/* Since we use builtin we have to assign slot to functions */
#if defined(SWIGPYTHON_BUILTIN)
  %feature("python:slot", "mp_length", functype="lenfunc") py__len__;
  %feature("python:slot", "mp_subscript", functype="binaryfunc") py__getitem__;
  %feature("python:slot", "mp_ass_subscript", functype="objobjargproc") py__setitem__;
  %feature("python:slot", "tp_str", functype="reprfunc") __str__;

%inline %{
#define SWIG_NewPointerObj_(ptr, type, flags) SWIG_InternalNewPointerObj(ptr, type, flags)
%}

#else
%inline %{
#define SWIG_NewPointerObj_(ptr, type, flags) SWIG_NewPointerObj_(ptr, type, flags)
%}


#endif // SWIGPYTHON_BUILTIN

%define def_pretty_print()
  void _repr_pretty_(PyObject *p, bool cycle) {
     const char *name = MY_CONCAT($parentclassname, __str__)($self);
      PyObject_CallMethodObjArgs(p, SWIG_Python_str_FromChar("text"), SWIG_Python_str_FromChar(name), NULL);
  }
%enddef

%define def_pretty_print_alloc()
  void _repr_pretty_(PyObject *p, bool cycle) {
     const char *name = MY_CONCAT($parentclassname, __str__)($self);
      PyObject_CallMethodObjArgs(p, SWIG_Python_str_FromChar("text"), SWIG_Python_str_FromChar(name), NULL);
      free((char*)name);
  }
%enddef

%extend BasisStatus {

  const char *__str__() {
     return rhp_basis_str($self->status);
  }

   def_pretty_print()
};

%newobject rhp_avar::__str__;
typedef struct rhp_avar {
   %extend {
  rhp_avar() { return rhp_avar_new(); }
  rhp_avar(unsigned size, unsigned start) { return rhp_avar_newcompact(size, start); }
  ~rhp_avar() { rhp_avar_free($self); }

  size_t py__len__() const { return rhp_avar_size($self); }

  const char *__str__() {
     char *str;
     IO_CALL_SWIG(asprintf(&str, "Abstract variable of type %s and size %u", rhp_avar_gettypename($self), rhp_avar_size($self)));
     return str;
     fail:
     return NULL;
  }

   def_pretty_print_alloc()

  rhp_idx_typed py__getitem__(size_t i) const {
     rhp_idx vi;
     int rc = rhp_avar_get($self, i, &vi);
     if (rc != RHP_OK || !valid_vi(vi)) {
       SWIG_exception(SWIG_IndexError, "Index out of bounds");
     }

     return vi;
fail:
   return IdxNA;
   }
/*  const rhp_idx py__setitem__(size_t i) const { */

   }
} Avar;

%newobject rhp_aequ::__str__;
typedef struct rhp_aequ {
   %extend {
  rhp_aequ() { return rhp_aequ_new(); }
  rhp_aequ(unsigned size, unsigned start) { return rhp_aequ_newcompact(size, start); }
  ~rhp_aequ() { rhp_aequ_free($self); }

  size_t py__len__() const { return rhp_aequ_size($self); }

  const char *__str__() {
     char *str;
     IO_CALL_SWIG(asprintf(&str, "Abstract equation of type %s and size %u", rhp_aequ_gettypename($self), rhp_aequ_size($self)));
     return str;
     fail:
     return NULL;
  }


   def_pretty_print_alloc()

  rhp_idx_typed py__getitem__(size_t i) const {
     rhp_idx ei;
     int rc = rhp_aequ_get($self, i, &ei);
     if (rc != RHP_OK || !valid_ei(ei)) {
       SWIG_exception(SWIG_IndexError, "Index out of bounds");
     }

     return ei;
fail:
   return IdxNA;
   }
   }
} Aequ;

%newobject rhp_mdl::__str__;
typedef struct rhp_mdl {
   %extend {
      rhp_mdl(unsigned backend) { return rhp_mdl_new(backend); } 
      rhp_mdl(const char *gms_controlfile) {
         struct rhp_mdl *mdl = rhp_gms_newfromcntr (gms_controlfile);
         if (!mdl) {
            SWIG_exception(SWIG_RuntimeError, "Couldn't create model");
         }

         return mdl;
         fail:
         return NULL;
      } 

      ~rhp_mdl() { rhp_mdl_free($self); }

     const char *__str__() {
        char *str;
        IO_CALL_SWIG(asprintf(&str, "%s Model named '%s' #%u", rhp_mdl_getbackendname($self), rhp_mdl_getname($self), rhp_mdl_getid($self)));
        return str;
        fail:
        return NULL;
     }

   def_pretty_print_alloc()

//     PyObject *getsolution(void) {
//        /* We have 4 cols: name, value, multiplier, basis status*/
//        /* (NPY_STRING, NPY_DOUBLE, NPY_DOUBLE, NPY_STRING) */
//
//        PyObject *dtypes = PyTuple_Pack(4, PyArray_DescrFromType(NPY_OBJECT),
//                                           PyArray_DescrFromType(NPY_DOUBLE),
//                                           PyArray_DescrFromType(NPY_DOUBLE),
//                                           PyArray_DescrFromType(NPY_OBJECT));
//
//        npy_intp dims[1] = {4};
//        PyObject *structured_array = PyArray_NewFromDescr(&PyArray_Type, dtypes, 1, dims, NULL, NULL, 0, NULL);
//     }

      PyObject *test_npy_from_C(void) {
         PyObject *pool_npy;
         unsigned n = 20;
         double *data = calloc(n, sizeof(double));
         C_to_target_lang1(pool_npy, n, data, TARGET_ERROR_VERBOSE);
         return pool_npy;
      }

     PyObject *getvariables(void) {
        unsigned n = rhp_mdl_nvars($self);
        PyObject *variables = PyTuple_New(n);

        if (!variables) { SWIG_exception_fail(SWIG_MemoryError, "malloc failed"); }

         for (unsigned i = 0; i < n; ++i) {
            VariableRef * vref = malloc(sizeof(VariableRef));
            if (!vref) { SWIG_exception_fail(SWIG_MemoryError, "malloc failed"); }

            vref->mdl = $self;
            vref->vi = i;

            PyObject *o = SWIG_NewPointerObj_(SWIG_as_voidptr(vref),$descriptor(VariableRef *), SWIG_POINTER_OWN | %newpointer_flags);
            if (!o) { goto fail; }

            int rc = PyTuple_SetItem(variables, i, o);
            if (rc != 0) { goto fail; /* Error should be set*/ }
         }

      return variables;
      fail: free(variables); return NULL;
     }


     void solve(const char *gdxname) {
        struct rhp_mdl *solver = rhp_mdl_new(RHP_BACKEND_GAMS_GMO);
        RHP_FAIL(rhp_process($self, solver), "Couldn't process the model");   
        RHP_FAIL(rhp_solve(solver), "Couldn't solve the model");   
        RHP_FAIL(rhp_postprocess(solver), "Couldn't report the solution");   
        RHP_FAIL(rhp_gms_writesol2gdx($self, gdxname), "Couldn't write the solution as gdx");
      fail:
     }

     void solve() {
        struct rhp_mdl *solver = rhp_mdl_new(RHP_BACKEND_GAMS_GMO);
        RHP_FAIL(rhp_process($self, solver), "Couldn't process the model");   
        RHP_FAIL(rhp_solve(solver), "Couldn't solve the model");   
        RHP_FAIL(rhp_postprocess(solver), "Couldn't report the solution");   
      fail:
     }

  }
} Model;


/* IMPORTANT: now attributes are MUTABLE by default!*/
%mutable;

/* Notes:
  - SWIG is unhappy to used the typedef, we need to use the struct name
  - %ignore must be placed before the function declaration
  */

%include attribute.i

/* This is from attribute.swg. We have to modify it to not use a define in C so that the %newobject directive works */
%define %my_attribute_custom_ptr(Class, AttributeType, AttributeName, GetMethodCall, SetMethodCall)
  %ignore Class::GetMethod();
  %ignore Class::GetMethod() const;
  #if #SetMethodCall != ""
      %ignore Class::SetMethod;
  #else
      %immutable Class::AttributeName;
   #endif
  %extend Class {
    AttributeType AttributeName;
  }
  %newobject %mangle(Class) ##_## AttributeName ## _get;
  %{
    static inline AttributeType* %mangle(Class) ##_## AttributeName ## _get(struct Class *self_) { return GetMethodCall(self_); }
  %}
  #if #SetMethodCall != ""
  %{
    static inline void %mangle(Class) ##_## AttributeName ## _set(struct Class *self_, AttributeType *val_) { return SetMethodCall(self_, val_); };
  %}
  #endif
%enddef

%define KLASS_SETTER(klass, klass_short, idxname, fnlast, member) 
%ignore klass ## _ ## member ## _set_;
%inline %{
static void klass ## _ ## member ## _set_(struct klass *o, double val) {
   CHK_RET(rhp_mdl_set ## klass_short ## fnlast(o->mdl, o->idxname, val));
   fail: ;
}
%}
%enddef

%define KLASS_GETTER(klass, klass_short, idxname, fnlast, member)
%ignore klass ## _ ## member ## _get_;
%inline %{
static double klass ## _ ## member ## _get_(struct klass *o) {
   double val;
   CHK_RET(rhp_mdl_get ## klass_short ## fnlast(o->mdl, o->idxname, &val));
   return val;
   fail: return NAN;
}
%}
%enddef

%define KLASS_GETSET(klass, klass_short, idxname, fnlast, member)
KLASS_SETTER(klass, klass_short, idxname, fnlast, member)
KLASS_GETTER(klass, klass_short, idxname, fnlast, member)
%attribute_custom(klass, double, member, dummy1, dummy2, klass ## _ ## member ## _get_(self_), klass ## _ ## member ## _set_(self_, val_));
%enddef

%define KLASS_GETONLY(klass, klass_short, idxname, fnlast, member)
KLASS_GETTER(klass, klass_short, idxname, fnlast, member)
%attribute_readonly(klass, double, member, dummy1, klass ## _ ## member ## _get_(self_));
%enddef


%define VAR_GETSET(fnlast, member) KLASS_GETSET(rhp_variable_ref, var, vi, fnlast, member) %enddef
%define EQU_GETSET(fnlast, member) KLASS_GETSET(rhp_equation_ref, equ, ei, fnlast, member) %enddef

%define VAR_GETONLY(fnlast, member) KLASS_GETONLY(rhp_variable_ref, var, vi, fnlast, member) %enddef

/* 1st arg: short short (the one used in function names)
   2nd arg: the attribute name in the target language
 */
VAR_GETONLY(lb, lo);
VAR_GETONLY(ub, up);

VAR_GETSET(val, value);
VAR_GETSET(mult, multiplier);

EQU_GETSET(val, value);
EQU_GETSET(mult, multiplier);

%attribute_readonly(rhp_variable_ref, const char*, name, dummy1, rhp_mdl_printvarname(self_->mdl, self_->vi));
%attribute_readonly(rhp_equation_ref, const char*, name, dummy1, rhp_mdl_printequname(self_->mdl, self_->ei));

%ignore var_getbasis;
%ignore var_setbasis;

%inline %{
static BasisStatus* var_getbasis(struct rhp_variable_ref *o) {
   BasisStatus *basis = malloc(sizeof(BasisStatus));
   if (!basis) { SWIG_exception_fail(SWIG_MemoryError, "malloc failed"); }

   int basis_status;
   CHK_RET(rhp_mdl_getvarbasis(o->mdl, o->vi, &basis_status));
   basis->status = basis_status;
   return basis;

   fail: return NULL;
}
static void var_setbasis(struct rhp_variable_ref *o, BasisStatus *basis) {
   CHK_RET(rhp_mdl_setvarbasis(o->mdl, o->vi, basis->status));
   fail: ;
}

static BasisStatus* equ_getbasis(struct rhp_equation_ref *o) {
   BasisStatus *basis = malloc(sizeof(BasisStatus));
   if (!basis) { SWIG_exception_fail(SWIG_MemoryError, "malloc failed"); }

   int basis_status;
   CHK_RET(rhp_mdl_getequbasis(o->mdl, o->ei, &basis_status));
   basis->status = basis_status;
   return basis;

   fail: return NULL;
}
static void equ_setbasis(struct rhp_equation_ref *o, BasisStatus *basis) {
   CHK_RET(rhp_mdl_setequbasis(o->mdl, o->ei, basis->status));
   fail: ;
}

static EquationRef* var_getdual(struct rhp_variable_ref *o) {
   EquationRef *dual = malloc(sizeof(EquationRef));
   if (!dual) { SWIG_exception_fail(SWIG_MemoryError, "malloc failed"); }
   dual->mdl = o->mdl;

   rhp_idx ei;
   CHK_RET(rhp_mdl_getvarperp(o->mdl, o->vi, &ei));
   dual->ei = ei;
   return dual;

   fail: return NULL;
}
static VariableRef* equ_getdual(struct rhp_equation_ref *o) {
   VariableRef *dual = malloc(sizeof(VariableRef));
   if (!dual) { SWIG_exception_fail(SWIG_MemoryError, "malloc failed"); }
   dual->mdl = o->mdl;

   rhp_idx vi;
   CHK_RET(rhp_mdl_getequperp(o->mdl, o->ei, &vi));
   dual->vi = vi;
   return dual;

   fail: return NULL;
}
%}

// Note: the newobject declaration does not seem to work. Might cause issue
%my_attribute_custom_ptr(rhp_variable_ref, BasisStatus, basis, var_getbasis, var_setbasis);
%my_attribute_custom_ptr(rhp_equation_ref, BasisStatus, basis, equ_getbasis, equ_setbasis);

%my_attribute_custom_ptr(rhp_variable_ref, EquationRef, dual, var_getdual, );
%my_attribute_custom_ptr(rhp_equation_ref, VariableRef, dual, equ_getdual, );

%extend struct rhp_variable_ref {

   const char *__str__() {
      return rhp_mdl_printvarname($self->mdl, $self->vi);
   }

   def_pretty_print()
};

%extend struct rhp_equation_ref {

   const char *__str__() {
      return rhp_mdl_printequname($self->mdl, $self->ei);
   }

   def_pretty_print()
};

// Note to self: since struct rhp_mathprgm is opaque, one needs to "declare" the struct there

%newobject rhp_mathprgm::__str__;
%ignore rhp_mathprgm::mdl;
typedef struct rhp_mathprgm {
%extend  {

   const char *__str__() {
      char *str;

%define FMT_COMMON "MathPrgm '%.*s' (ID #%u) of type %s with %u vars, %u constraints. " %enddef

      unsigned id = rhp_mp_getid($self);
      unsigned sense = rhp_mp_getsense($self);
      unsigned nvars = rhp_mp_nvars($self);
      unsigned ncons = rhp_mp_ncons($self);
      const char *name = rhp_mp_getname($self);
      const char *sensestr = rhp_sensestr(sense);

      switch(sense) {
      case RHP_MIN: case RHP_MAX: {
         rhp_idx objvar = rhp_mp_getobjvar($self);
         rhp_idx objequ = rhp_mp_getobjequ($self);
         const struct rhp_mdl *mdl = rhp_mp_getmdl($self);
         IO_CALL_SWIG(asprintf(&str, FMT_COMMON "Objvar is %s; Objequ is %s",
                          (int)strlen(name), name, id, sensestr, nvars, ncons,
                          rhp_mdl_printvarname(mdl, objvar),
                          rhp_mdl_printequname(mdl, objequ)));
         break;
      }
      case RHP_FEAS: {
         unsigned nvifunc = rhp_mp_nmatched($self);
         IO_CALL_SWIG(asprintf(&str, FMT_COMMON "%u matched variables and equations\n",
                          (int)strlen(name), name, id, sensestr, nvars, ncons, nvifunc));
         break;
      }
         default:
         SWIG_exception_fail(SWIG_RuntimeError, "unsupported MathPrgm sense");
      }

      return str;
      fail: return NULL;
   }

   def_pretty_print_alloc()
}

} MathPrgm;

// Note to self: since struct rhp_nash_equilibrium is opaque, one needs to "declare" the struct there

%newobject rhp_nash_equilibrium::__str__;
%ignore rhp_nash_equilibrium::mdl;
typedef struct rhp_nash_equilibrium {
%extend  {

   const char *__str__() {
      char *str;

      unsigned id = rhp_mpe_getid($self);
      unsigned nchildren = rhp_mpe_getnumchildren($self);
      const char *name = rhp_mpe_getname($self);

      IO_CALL_SWIG(asprintf(&str, "Nash Equilibrium '%.*s' (ID #%u) with %u children.",
                          (int)(name ? strlen(name) : 6), name ? name : "NONAME", id, nchildren))
      return str;
      fail: return NULL;
   }

   def_pretty_print_alloc()
}

} NashEquilibrium;
