%module reshop



%include start.i

%include linalg_common.i
%include matrix.i

%include pyobjs_custom.i
%include fun_custom.i

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

/* Overload `rhp_add_XXX_YYY` with `rhp_add_XXX_YYY_named` */
%include rename_named.i

/* Overload (set|get)_option */

%ignore rhp_mdl_setopt_b;
%ignore rhp_mdl_setopt_d;
%ignore rhp_mdl_setopt_i;
%ignore rhp_mdl_setopt_s;

%ignore rhp_mdl_getopt_b;
%ignore rhp_mdl_getopt_d;
%ignore rhp_mdl_getopt_i;
%ignore rhp_mdl_getopt_s;

/* ----------------------------------------------------------------------
 * Basic exception handling, for setter: a tls global variable is set
 * to indicate the error. The indicator is then checked against it.
 *
 * The use of this exception handler is controlled by %allowexception
 * ---------------------------------------------------------------------- */
%define %handle_exception(attribute)
  %exception attribute {
     $action
  
     if (exception_code != RHP_OK) {
        int ecode = exception_code;
        exception_code = RHP_OK; 
        if (ecode == -1) {
           SWIG_fail; /* everything has been set by the callee */
        }

        if (exception_msg) {
           SWIG_exception(errcode_rhp2swig(ecode), exception_msg);
         } else {
            SWIG_exception(errcode_rhp2swig(ecode), "ERROR: exception present without message!");
         }
     }
  }
%enddef

%handle_exception(set_option);
%handle_exception(mdl_set_option);
void set_option(const char *name, SWIG_Object o);
void mdl_set_option(struct rhp_mdl *mdl, const char *name, SWIG_Object o);

/* ----------------------------------------------------------------------
 * START typemaps:
   1. Classical typemaps
   2. Custom typemaps
 * ---------------------------------------------------------------------- */

// These are the classical Numpy -> double *
%apply double *rhp_dblin { const double *coeffs };

%apply ( int DIM1, double* IN_ARRAY1 ) { (unsigned n_vals, double *vals) };
%apply double IN_ARRAY1[ANY] { double *lbs, double *ubs };

%apply (size_t nnz, unsigned *i, unsigned *j, double *x ) { (size_t nnz, rhp_idx *i, rhp_idx *j, double *x) };

/* For option setting */
%apply bool { unsigned char opt, unsigned char bval};

%apply double *OUTPUT { double *dval };
%apply int *OUTPUT      { int *ival };
%apply unsigned *OUTPUT { unsigned *type };


%typemap(in,numinputs=0,noblock=1) const char** str (char* temp) {
   $1 = &temp;
}

%typemap(argout,noblock=1) const char** str {
   %set_output(SWIG_FromCharPtr(temp$argnum));
}

/* ----------------------------------------------------------------------
 * START custom typemaps
 * ---------------------------------------------------------------------- */

/* numinputs=0 make the argument disappear from the target language */
/* noblock=1 prevents { } from being added to the generated code */
%define %abstract_types(init_arg, Type)
 %typemap(in,numinputs=0,noblock=1)
   Type *vout ($*1_ltype temp), 
   Type &vout ($*1_ltype temp) {
   init_arg(temp);
   $1 = &temp;
 }
 %typemap(argout,noblock=1) Type *vout, Type &vout {
    int new_flags = (SWIG_POINTER_OWN | %newpointer_flags);
    %set_output(SWIG_NewPointerObj_((void*)($1), $1_descriptor, new_flags));
 }
%enddef

/* TODO: what do we want to achieve here? */
abstract_types(rhp_avar_new, struct rhp_avar);
abstract_types(rhp_aequ_new, struct rhp_aequ);

%typemap(in,numinputs=0,noblock=1) 
  int *ei ($*1_ltype temp, int res = SWIG_TMPOBJ) {
  $1 = &temp;
}

%typemap(argout,noblock=1) int *ei {
  EquationRef *ei_ref = (EquationRef *)PyMem_RawMalloc(sizeof(EquationRef));
  if (!ei_ref) { SWIG_fail; }
  ei_ref->ei = temp$argnum;
  ei_ref->mdl = arg1; /* Automagic: the first argument is always the model */

  %set_output(SWIG_NewPointerObj_(ei_ref, $descriptor(EquationRef*), (SWIG_POINTER_OWN | %newpointer_flags)));
}

%apply int * ei { rhp_idx * objequ };

/* ----------------------------------------------------------------------
 * Typemap int *vi -> Variable Ref
 * ---------------------------------------------------------------------- */
%typemap(in,numinputs=0,noblock=1) 
  rhp_idx *vi ($*1_ltype temp, int res = SWIG_TMPOBJ) {
  $1 = &temp;
}

%typemap(argout,noblock=1) rhp_idx *vi {
  VariableRef *vi_ref = (VariableRef *)PyMem_RawMalloc(sizeof(VariableRef));
  if (!vi_ref) { SWIG_fail; }
  vi_ref->vi = temp$argnum;
  vi_ref->mdl = arg1; /* Automagic: the first argument is always the model */

  %set_output(SWIG_NewPointerObj_(vi_ref, $descriptor(VariableRef*), (SWIG_POINTER_OWN | %newpointer_flags)));
}

%apply rhp_idx * vi { rhp_idx * objvar };

/* ----------------------------------------------------------------------
 * Typemap int vi -> either an integer or a VariableRef
 * Typemap int ei -> either an integer or a EquationRef
 *
 * TODO: do we need $disown in the flags of ConvertPtr?
 * ---------------------------------------------------------------------- */
%typemap(in,noblock=1) rhp_idx vi (void *argp, int res, int val) {
   if (SWIG_IsOK(SWIG_ConvertPtr($input, &argp, $descriptor(VariableRef*), 0))) {
      VariableRef *vref = (VariableRef *)argp;
      $1 = vref->vi;
   } else {
      res = SWIG_AsVal_int($input, &val);
      if (!SWIG_IsOK(res)) {
         SWIG_exception_fail(SWIG_ArgError(res), "in method '" "$symname" "', argument "
                       "$argnum"" of type '" "$type""'");
      }
      $1 = val;
   }
}
%typemap(in,noblock=1) rhp_idx ei (void *argp, int res, int val) {
   if (SWIG_IsOK(SWIG_ConvertPtr($input, &argp, $descriptor(EquationRef*), 0))) {
      EquationRef *eref = (EquationRef *)argp;
      $1 = eref->ei;
   } else {
      res = SWIG_AsVal_int($input, &val);
      if (!SWIG_IsOK(res)) {
         SWIG_exception_fail(SWIG_ArgError(res), "in method '" "$symname" "', argument "
                       "$argnum"" of type '" "$type""'");
      }
      $1 = val;
   }
}

/* ----------------------------------------------------------------------
 * Typemap int *bstat -> return one of the basis constant objects
 * ---------------------------------------------------------------------- */
%typemap(in,numinputs=0,noblock=1) 
  int *basis_status ($*1_ltype temp, int res = SWIG_TMPOBJ) {
  $1 = &temp;
}

%typemap(argout,noblock=1) int *basis_status {
#ifdef SWIGPYTHON
   PyObject *bstat = basisstatus_getobj(temp$argnum) ;
   if (!bstat) { SWIG_fail; }

   %set_output(bstat);
#else

   %set_output(SWIG_From_int(temp$argnum));

#endif
}

/* ----------------------------------------------------------------------
 * Typemap double* and rhp_idx* as output parameters
 * ---------------------------------------------------------------------- */
%typemap(in,numinputs=0,noblock=1) 
  double *rhpDblOut ($*1_ltype temp, int res = SWIG_TMPOBJ) {
  $1 = &temp;
}

%typemap(argout,noblock=1) double *rhpDblOut {
   %set_output(SWIG_From_double(temp$argnum));
}

%apply double *rhpDblOut { double *val, double *mult }

%typemap(in,numinputs=0,noblock=1) (double *lb, double *ub) 
   (double lb_, double ub_) {
      $1 = &lb_;
      $2 = &ub_;
}

%typemap(argout,noblock=1) (double *lb, double *ub)  {
   PyObject* py_dict = PyDict_New();
   if (!py_dict) { SWIG_fail; }

   PyObject* py_lb = PyFloat_FromDouble(lb_$argnum);
   PyObject* py_ub = PyFloat_FromDouble(ub_$argnum);

   if (!py_lb || !py_ub) { SWIG_fail; }

   int rc1 = PyDict_SetItemString(py_dict, "lb", py_lb);
   int rc2 = PyDict_SetItemString(py_dict, "ub", py_ub);

   if (!(SWIG_IsOK(rc1) && SWIG_IsOK(rc2))) { SWIG_fail; }

   %set_output(py_dict);
}

%typemap(in,numinputs=0,noblock=1) 
  rhp_idx *rhpRhpIdxOut ($*1_ltype temp, int res = SWIG_TMPOBJ) {
  $1 = &temp;
}

%typemap(argout,noblock=1) rhp_idx *rhpRhpIdxOut {
   %set_output(SWIG_From_int(temp$argnum));
}

%apply rhp_idx *rhpRhpIdxOut { rhp_idx *idx }

/* ----------------------------------------------------------------------
 * Typemap (model|solve)stat -> Custom (Model|Solve)Status
 * ---------------------------------------------------------------------- */
%typemap(in,numinputs=0,noblock=1) 
  int *modelstat ($*1_ltype temp, int res = SWIG_TMPOBJ) {
  $1 = &temp;
}

%typemap(argout,noblock=1) int *modelstat {
  ModelStatus *mstat = (ModelStatus *)PyMem_RawMalloc(sizeof(ModelStatus));
  if (!mstat) { SWIG_fail; }
  mstat->code = temp$argnum;
  mstat->mdl = arg1; /* Automagic: the first argument is always the model */

  %set_output(SWIG_NewPointerObj_(mstat, $descriptor(ModelStatus*), (SWIG_POINTER_OWN | %newpointer_flags)));
}

%typemap(in,numinputs=0,noblock=1) 
  int *solvestat ($*1_ltype temp, int res = SWIG_TMPOBJ) {
  $1 = &temp;
}

%typemap(argout,noblock=1) int *solvestat {
  SolveStatus *sstat = (SolveStatus *)PyMem_RawMalloc(sizeof(SolveStatus));
  if (!sstat) { SWIG_fail; }
  sstat->code = temp$argnum;
  sstat->mdl = arg1; /* Automagic: the first argument is always the model */

  %set_output(SWIG_NewPointerObj_(sstat, $descriptor(SolveStatus*), (SWIG_POINTER_OWN | %newpointer_flags)));
}



/* ----------------------------------------------------------------------
 * Typemap for an output array whose size depend on the a(var|equ) given as argument
 * ---------------------------------------------------------------------- */
%typemap(in,numinputs=0,noblock=1)
  double *rhpVecOutAvar (SN_ARRAY_TYPE* array = NULL) {
     /* Do nothing as arg2 might be not initialized; the data is created in the check typemap */
  }

// HACK: doing the allocation in 'check' as with 'numinputs=0', swig 4.3.1 places the 'in' part before the argument unpacking
%typemap(check) double *rhpVecOutAvar {
   C_to_target_lang1_alloc($1, array$argnum, rhp_avar_size(arg2), SWIG_fail)
}

%typemap(argout,noblock=1) double *rhpVecOutAvar {
     %set_output((SWIG_Object)array$argnum);
  }

%typemap(in,numinputs=0,noblock=1)
  double *rhpVecOutAequ (SN_ARRAY_TYPE* array = NULL) {
     /* Do nothing as arg2 might be not initialized; the data is created in the check typemap */
  }

%typemap(check) double *rhpVecOutAequ {
   C_to_target_lang1_alloc($1, array$argnum, rhp_aequ_size(arg2), SWIG_fail)
}

%typemap(argout,noblock=1) double *rhpVecOutAequ {
     %set_output((SWIG_Object)array$argnum);
  }

%apply (double * rhpVecOutAvar) { double *vars_val, double *vars_mult };
%apply (double * rhpVecOutAequ) { double *equs_val, double *equs_mult };



%typemap(in,numinputs=0,noblock=1)
   struct rhp_avar *vout ($1_ltype temp), 
   struct rhp_avar &vout ($1_ltype temp) {
   temp = rhp_avar_new();
   $1 = temp;
}
%typemap(argout,noblock=1) struct rhp_avar *vout, struct rhp_avar &vout {
    int new_flags = (SWIG_POINTER_OWN | %newpointer_flags);
    %set_output(SWIG_NewPointerObj_((void*)($1), $1_descriptor, new_flags));
}
%typemap(in,numinputs=0,noblock=1)
   struct rhp_aequ *eout ($1_ltype temp), 
   struct rhp_aequ &eout ($1_ltype temp) {
   temp = rhp_aequ_new();
   $1 = temp;
}
%typemap(argout,noblock=1) struct rhp_aequ *eout, struct rhp_aequ &eout {
    int new_flags = (SWIG_POINTER_OWN | %newpointer_flags);
    %set_output(SWIG_NewPointerObj_((void*)($1), $1_descriptor, new_flags));
}

%typemap(in,numinputs=0,noblock=1) struct rhp_ovfdef **ovf_def ($*1_ltype temp) {
  $1 = &temp;
}
%typemap(argout,noblock=1) struct rhp_ovfdef **ovf_def {
/* This is owned by reshop */
  int new_flags = 0;
  %set_output(SWIG_NewPointerObj((void*)(*$1), $*1_descriptor, new_flags));
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

%apply ( int DIM1, rhp_idx* IN_ARRAY1 ) { (unsigned size, rhp_idx *vis) };
%apply ( int DIM1, rhp_idx* IN_ARRAY1 ) { (unsigned size, rhp_idx *eis) };
%apply ( int DIM1, rhp_idx* IN_ARRAY1 ) { (unsigned size, rhp_idx *list) };

/* ----------------------------------------------------------------------
 * START specialized typemaps
 * ---------------------------------------------------------------------- */

/* getspecialfloats */
%typemap(in,numinputs=1,noblock=1) (const struct rhp_mdl *mdl, double * minf, double * pinf, double * nan) (void *argp, double minf_, double pinf_, double nan_) {
//  if (!SWIG_Python_UnpackTuple(args, "mdl_getvarperp", 2, 2, swig_obj)) SWIG_fail;
  int res = SWIG_ConvertPtr(swig_obj[0], &argp,$1_descriptor, 0 |  0 );
  if (!SWIG_IsOK(res)) {
    SWIG_exception_fail(SWIG_ArgError(res), "in method '" "$symname" "', argument " "$argnum"" of type '" "$type""'");
  }
  $1 = (struct rhp_mdl *)(argp1);
   $2 = ($2_ltype)&minf_;
   $3 = ($3_ltype)&pinf_;
   $4 = ($4_ltype)&nan_;
}

%typemap(argout) (const struct rhp_mdl *mdl, double *minf, double *pinf, double* nan) {
   PyObject* py_dict = PyDict_New();
   if (!py_dict) { SWIG_fail; }

   PyObject* py_minf = PyFloat_FromDouble(minf_$argnum);
   PyObject* py_pinf = PyFloat_FromDouble(pinf_$argnum);
   PyObject* py_nan = PyFloat_FromDouble(nan_$argnum);

   if (!py_minf || !py_pinf || !py_nan) { SWIG_fail; }

   int rc1 = PyDict_SetItemString(py_dict, "-inf", py_minf);
   int rc2 = PyDict_SetItemString(py_dict, "+inf", py_pinf);
   int rc3 = PyDict_SetItemString(py_dict, "NaN", py_nan);

   if (!(SWIG_IsOK(rc1) && SWIG_IsOK(rc2) && SWIG_IsOK(rc3))) { SWIG_fail; }

   %set_output(py_dict);
}


// NOTE: this is a hack to not create too many BasisStatus objects
// This may fail hard, we need to be careful
%typemap(out) BasisStatus * {
   $result = (SWIG_Object)$1;
}

/* See http://www.swig.org/Doc4.0/SWIGDocumentation.html#Python_nn44 for more options */
%include "exception.i"
%typemap(out, noblock=1) int {
   /* Return code should not be an output */
}

%apply int { Int };

%typemap(ret) int {
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

// Remove typemap used to intercept the return code
//%typemap(out) int;

%apply Int { int };

/* Since we use builtin we have to assign slot to functions */
#if defined(SWIGPYTHON_BUILTIN)
/* The next 3 are for the mapping protocol (PyMappingMethods) */
%feature("python:slot", "mp_length", functype="lenfunc") __len__;
%feature("python:slot", "mp_subscript", functype="binaryfunc") __getitem__;
%feature("python:slot", "mp_ass_subscript", functype="objobjargproc") __setitem__;
%feature("python:slot", "sq_contains", functype="objobjproc") __contains__;

%feature("python:slot", "tp_str", functype="reprfunc") __str__;

%feature("python:tp_iter") rhp_avar "rhp_avar_iter";
%feature("python:tp_iter") rhp_aequ "rhp_aequ_iter";


%immutable;
%include "python_structs.h"

%inline %{
#define SWIG_NewPointerObj_(ptr, type, flags) SWIG_InternalNewPointerObj(ptr, type, flags)
%}

#else
%inline %{
#define SWIG_NewPointerObj_(ptr, type, flags) SWIG_NewPointerObj(ptr, type, flags)
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


/* ----------------------------------------------------------------------
 * Create a nice "BasisStatus" object. Since, there is only a finite
 * number of basis statuses, we just create these object as constant.
 * Then, we can refer to them rather than create new instances
 * ---------------------------------------------------------------------- */

%constant struct BasisStatus BasisUnset = BasisUnsetObj;
%constant struct BasisStatus BasisLower = BasisLowerObj;
%constant struct BasisStatus BasisUpper = BasisUpperObj;
%constant struct BasisStatus BasisBasic = BasisBasicObj;
%constant struct BasisStatus BasisSuperBasic = BasisSuperBasicObj;
%constant struct BasisStatus BasisFixed = BasisFixedObj;
%constant struct BasisStatus BasisInvalid = BasisInvalidObj;


%extend BasisStatus {

  const char *__str__() {
     return rhp_basis_str($self->status);
  }

   def_pretty_print()
};

/* This is for the Python buffer protocol (PyBufferProcs) */
%define equvar_add_buffer_iface(Type, type)
%feature("python:bf_releasebuffer")     rhp_##type #Type "_py__bf_releasebuffer";
%feature("python:bf_getbuffer")         rhp_##type #Type "_py__bf_getbuffer";
%wrapper %{
    SWIGINTERN int Type##_py__bf_getbuffer(PyObject *exporter, Py_buffer *view, int flags) {
    SwigPyObject *swigobj = (SwigPyObject *)exporter;

    // Sanity checks
    if (!swigobj->ptr) {
        PyErr_SetString(PyExc_BufferError, "SWIG pointer is NULL.");
        return -1;
    }

    struct rhp_##type *obj = (struct rhp_##type*)swigobj->ptr;

    size_t sz = rhp_##type##_gettype(obj);
    unsigned typ = rhp_##type##_gettype(obj);
    bool ownmem = rhp_##type ##_ownmem(obj);

    if (typ != RHP_EQUVAR_LIST && typ != RHP_EQUVAR_SORTEDLIST) {
       PyErr_SetString(PyExc_BufferError, #Type " object must have type list");
       return -1;
    }

   rhp_idx *list;
    int rc = rhp_##type##_get_list(obj, &list);
    if (rc != RHP_OK) {
       PyErr_SetString(PyExc_BufferError, "Could not get list array from " #Type);
       return -1;
    }

    view->buf = list;
    view->shape = PyMem_RawMalloc(sizeof(Py_ssize_t));

    // Populate the Py_buffer struct
    view->obj = exporter;
    view->len = sz * sizeof(rhp_idx);
    view->readonly = typ == RHP_EQUVAR_SORTEDLIST || !ownmem; // The data is writable
    view->itemsize = sizeof(rhp_idx);
    view->format = "i"; // 'i' for signed integer (use "I" for unsigned). TODO: rhp_idx change
    view->ndim = 1;
    view->shape[0] = sz;
    view->strides = &view->itemsize; // Stride for 1D C array is just the item size
    view->suboffsets = NULL;
    view->internal = view->shape;

    Py_INCREF(exporter); // Increment ref count of the object

    return 0; // 0 indicates success
   }

   SWIGINTERN void Type##_py__bf_releasebuffer(PyObject *exporter, Py_buffer *view) {
      free(view->internal);
   }
%}
%enddef

equvar_add_buffer_iface(Avar,avar);
equvar_add_buffer_iface(Aequ,aequ);

%newobject rhp_avar::__str__;
typedef struct rhp_avar {
   %extend {
  rhp_avar() { return rhp_avar_new(); }
  rhp_avar(unsigned size, unsigned start) { return rhp_avar_newcompact(size, start); }
  ~rhp_avar() { rhp_avar_free($self); }

  size_t __len__() const { return rhp_avar_size($self); }

  const char *__str__() {
     char *str;
     IO_CALL_SWIG(asprintf(&str, "Abstract variable of type '%s' and size %u", rhp_avar_gettypename($self), rhp_avar_size($self)));
     return str;
     fail:
     return NULL;
  }

   def_pretty_print_alloc()

  rhp_idx_typed __getitem__(size_t i) const {
     rhp_idx vi;
     int rc = rhp_avar_get($self, i, &vi);
     if (rc != RHP_OK || !valid_vi(vi)) {
       SWIG_exception(SWIG_IndexError, "Index out of bounds");
     }

     return vi;
fail:
   return IdxNA;
   }

  short __contains__(rhp_idx vi) const {
     short rc = rhp_avar_contains($self, vi);
     // 1 -> contains, 0 -> no, -1 -> error
     return rc == 1 ? 1 : (rc == 0 ? 0 : -1);
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

  size_t __len__() const { return rhp_aequ_size($self); }

  const char *__str__() {
     char *str;
     IO_CALL_SWIG(asprintf(&str, "Abstract equation of type '%s' and size %u", rhp_aequ_gettypename($self), rhp_aequ_size($self)));
     return str;
     fail:
     return NULL;
  }


   def_pretty_print_alloc()

  rhp_idx_typed __getitem__(size_t i) const {
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

%handle_exception(rhp_mdl::solve);

%newobject rhp_mdl::__str__;
typedef struct rhp_mdl {
   %extend {

      rhp_mdl(unsigned backend) { return rhp_mdl_new(backend); } 

      rhp_mdl(const char *gms_controlfile) {
         struct rhp_mdl *mdl;
         int rc = rhp_gms_newfromcntr (gms_controlfile, &mdl);
         if (!mdl) {
            SWIG_exception(SWIG_RuntimeError, "Couldn't create model");
         }

         if (rc) {
            rhp_mdl_free(mdl);
            SWIG_exception(SWIG_RuntimeError, "Couldn't fully load the model");
         }

         return mdl;
         fail:
         return NULL;
      } 

      ~rhp_mdl() { rhp_mdl_free($self); }

     const char *__str__() {
        char *str;
        IO_CALL_SWIG(asprintf(&str, "%s Model named '%s' #%u with %u variables and %u equations",
                                     rhp_mdl_getbackendname($self), rhp_mdl_getname($self),
                                     rhp_mdl_getid($self), rhp_mdl_nvars($self), rhp_mdl_nequs($self)));
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

        if (!variables) { SWIG_exception_fail(SWIG_MemoryError, "Tuple creation failed"); }

         for (unsigned i = 0; i < n; ++i) {
            VariableRef * vref = PyMem_RawMalloc(sizeof(VariableRef));
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
      fail: exception_code = -1;
     }

     void solve() {
        struct rhp_mdl *solver = rhp_mdl_new(RHP_BACKEND_GAMS_GMO);
        RHP_FAIL(rhp_process($self, solver), "Couldn't process the model");   
        RHP_FAIL(rhp_solve(solver), "Couldn't solve the model");   
        RHP_FAIL(rhp_postprocess(solver), "Couldn't report the solution");   
      fail: ;
     }

  }
} Model;

/* ----------------------------------------------------------------------
 * Custom attributes. Right now only use for VariableRef/EquationRef
 * ---------------------------------------------------------------------- */


/* IMPORTANT: now attributes are MUTABLE by default!*/
// FIXME: why do we need that
%mutable;

/* Notes:
  - SWIG is unhappy to used the typedef, we need to use the struct name
  - %ignore must be placed before the function declaration
  */

%include attribute.i

/* This is from attribute.swg. We have to modify it to not use a define in C so that the %newobject directive works */
%define %my_attribute_custom_ptr(Class, AttributeType, AttributeName, GetMethodCall, SetMethodCall)
  %handle_exception(Class::AttributeName)
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

/* This defines a setter based on "rhp_mdl_getXXX" for double */
%define KLASS_SETTER(klass, klass_short, idxname, fnlast, member) 
%ignore klass ## _ ## member ## _set_;
%inline %{
static void klass ## _ ## member ## _set_(struct klass *o, double val) {
   CHK_RET_EXCEPTION(rhp_mdl_set ## klass_short ## fnlast(o->mdl, o->idxname, val));
}
%}
%enddef

/* This defines a getter based on "rhp_mdl_getXXX" for double */
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
%handle_exception(klass::member)
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

//%attribute_readonly(rhp _mdl, const char*, name, dummy1, rhp_mdl_printequname(self_->mdl, self_->ei));


%ignore equ_getbasis;
%ignore equ_getdual;
%ignore equ_setbasis;
%ignore var_getbasis;
%ignore var_getdual;
%ignore var_setbasis;


%inline %{
static PyObject* var_getbasis(struct rhp_variable_ref *o) {
   int basis_status;
   CHK_RET(rhp_mdl_getvarbasis(o->mdl, o->vi, &basis_status));
   return basisstatus_getobj(basis_status);

   fail: return NULL;
}
static void var_setbasis(struct rhp_variable_ref *o, BasisStatus *basis) {
   CHK_RET_EXCEPTION(rhp_mdl_setvarbasis(o->mdl, o->vi, basis->status));
}

static PyObject* equ_getbasis(struct rhp_equation_ref *o) {
   int basis_status;
   CHK_RET(rhp_mdl_getequbasis(o->mdl, o->ei, &basis_status));
   return basisstatus_getobj(basis_status);

   fail: return NULL;
}
static void equ_setbasis(struct rhp_equation_ref *o, BasisStatus *basis) {
   CHK_RET_EXCEPTION(rhp_mdl_setequbasis(o->mdl, o->ei, basis->status));
}

static EquationRef* var_getdual(struct rhp_variable_ref *o) {
   EquationRef *dual = PyMem_RawMalloc(sizeof(EquationRef));
   if (!dual) { SWIG_exception_fail(SWIG_MemoryError, "malloc failed"); }
   dual->mdl = o->mdl;

   rhp_idx ei;
   CHK_RET(rhp_mdl_getvarperp(o->mdl, o->vi, &ei));
   dual->ei = ei;
   return dual;

   fail: return NULL;
}
static VariableRef* equ_getdual(struct rhp_equation_ref *o) {
   VariableRef *dual = PyMem_RawMalloc(sizeof(VariableRef));
   if (!dual) { SWIG_exception_fail(SWIG_MemoryError, "malloc failed"); }
   dual->mdl = o->mdl;

   rhp_idx vi;
   CHK_RET(rhp_mdl_getequperp(o->mdl, o->ei, &vi));
   dual->vi = vi;
   return dual;

   fail: return NULL;
}
%}


%handle_exception(rhp_variable_ref::BasisStatus)
%handle_exception(rhp_equation_ref::BasisStatus)



// Note: the newobject declaration does not seem to work. Might cause issue
//%my_attribute_custom_ptr(rhp_variable_ref, BasisStatus, basis, var_getbasis, var_setbasis);
//%my_attribute_custom_ptr(rhp_equation_ref, BasisStatus, basis, equ_getbasis, equ_setbasis);
%attribute_custom(rhp_variable_ref, BasisStatus, basis, var_getbasis, var_setbasis, var_getbasis(self_), var_setbasis(self_, val_));
%attribute_custom(rhp_equation_ref, BasisStatus, basis, equ_getbasis, equ_setbasis, equ_getbasis(self_), equ_setbasis(self_, val_));

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



/* ----------------------------------------------------------------------
 * MP and Nash objects
 * ---------------------------------------------------------------------- */


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

/* Again, we need to ignore the exit code */
%typemap(out) int {
   /* Return code should not be an output */
}

/* Again, we need to ignore the exit code */
%typemap(out,noblock=1,optimal=1) const int & SMARTPOINTER {
   /* Return code should not be an output */
}

%typemap(ret) int {
  if ($1 != RHP_OK) {
    const char *msg = rhp_status_descr($1);
    SWIG_exception(SWIG_RuntimeError, msg);
  }
  if (!$result) {
     $result = VOID_Object;
  }
}

%include pyobj_methods.i

