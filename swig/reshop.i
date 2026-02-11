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
%rename(Vars)              rhp_avar;
%rename(Equs)              rhp_aequ;
%rename(MathPrgm)          rhp_mathprgm;
%rename(NashEquilibrium)   rhp_nash_equilibrium;
%rename(BackendType)       RhpBackendType;

%rename(VariableRef)       rhp_variable_ref;
%rename(EquationRef)       rhp_equation_ref;

/* Overload `rhp_add_XXX_YYY` with `rhp_add_XXX_YYY_named` */
%include generated/rename_named.i

/* Give our docstrings */
%include generated/reshop_docs.i
%include generated/pyobj_methods_docstring.i
%feature("autodoc", "2") rhp_backendtype;
%feature("autodoc", "2") rhp_mp_type;
%feature("docstring") rhp_variable_ref::dual "Get the equation dual to this variable";
%feature("docstring") rhp_equation_ref::dual "Get the variable dual to this equation";
%feature("docstring") *::lo "Get the lower bound";
%feature("docstring") *::up "Get the upper bound";
%feature("docstring") *::basis "Get the basis status";
%feature("docstring") *::dual "Get the dual multiplier value";
%feature("docstring") *::marginal "Get the marginal value";
%feature("docstring") *::level "Get the (level) value";

/* Overload (set|get)_option */

%ignore rhp_mdl_setopt_b;
%ignore rhp_mdl_setopt_d;
%ignore rhp_mdl_setopt_i;
%ignore rhp_mdl_setopt_s;

%ignore rhp_mdl_getopt_b;
%ignore rhp_mdl_getopt_d;
%ignore rhp_mdl_getopt_i;
%ignore rhp_mdl_getopt_s;

/* Useless, needs a FILE * stream */
%ignore rhp_empdag_writeDOT;

/* ----------------------------------------------------------------------
 * Basic exception handling, for setter: a tls global variable is set
 * to indicate the error. The indicator is then checked against it.
 *
 * The use of this exception handler is controlled by %allowexception
 * ---------------------------------------------------------------------- */
%define %handle_exception(attribute)
  %exception attribute {
     $action
  
     if (rhp_exception_code != RHP_OK) {
        int ecode = rhp_exception_code;
        rhp_exception_code = RHP_OK; 
        if (ecode == -1) {
           SWIG_fail; /* everything has been set by the callee */
        }

        if (rhp_exception_msg) {
           SWIG_exception(errcode_rhp2swig(ecode), rhp_exception_msg);
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

%apply ( int DIM1, double* IN_ARRAY1 ) {  (unsigned size, double *vec) };
%apply ( int DIM1, rhp_idx* IN_ARRAY1 ) { 
                                          (unsigned size, rhp_idx *vis),
                                          (unsigned size, rhp_idx *vis) };
%apply ( int DIM1, rhp_idx* IN_ARRAY1 ) { (unsigned size, rhp_idx *vlist) };

//%apply ( unsigned* DIM1, double** ARGOUTVIEW_ARRAY1 ) { (unsigned *len, double **vals) };
//%apply ( unsigned* DIM1, rhp_idx** ARGOUTVIEW_ARRAY1 ) { (unsigned *len, rhp_idx **idxs) };

%apply double IN_ARRAY1[ANY] { double *lbs, double *ubs };

%apply (size_t nnz, unsigned *i, unsigned *j, double *x ) { (size_t nnz, rhp_idx *i, rhp_idx *j, double *x) };

/* For option setting */
%apply bool { unsigned char opt, unsigned char boolval};

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

%typemap(in,numinputs=0)
  (unsigned* len, rhp_idx **idxs, double **vals)
  (unsigned  dim_temp, double*  data_temp = NULL, rhp_idx *idx_temp = NULL )
{
  $1 = &dim_temp;
  $2 = &idx_temp;
  $3 = &data_temp;
}
%typemap(argout,
         fragment="NumPy_Backward_Compatibility")
  (unsigned* len, rhp_idx **idxs, double **vals)
{
  struct rhp_linear_equation *equlin = new_rhp_linear_equation(arg1,*$1,*$2,*$3);
  %set_output(SWIG_NewPointerObj_(SWIG_as_voidptr(equlin), $descriptor(rhp_linear_equation*), SWIG_POINTER_OWN | %newpointer_flags));
}


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

%apply rhp_idx vi { rhp_idx objvar };
%apply rhp_idx ei { rhp_idx objequ };

/* ----------------------------------------------------------------------
 * Typemap int *bstat -> return one of the basis constant objects
 * ---------------------------------------------------------------------- */
%typemap(in,numinputs=0,noblock=1) 
  int *basis_info ($*1_ltype temp, int res = SWIG_TMPOBJ) {
  $1 = &temp;
}

%typemap(argout,noblock=1) int *basis_info {
#ifdef SWIGPYTHON
   PyObject *bstat = basisstatus_getobj(temp$argnum) ;
   if (!bstat) { SWIG_fail; }

   %set_output(bstat);

#else

   %set_output(SWIG_From_int(temp$argnum));

#endif
}

/* ----------------------------------------------------------------------
 * Typemap enum rhp_backendtype -> constant object
 * ---------------------------------------------------------------------- */
%typemap(out,noblock=1) enum rhp_backendtype {
#ifdef SWIGPYTHON
   PyObject *o = backendtype_getobj($1);
   if (!o) { SWIG_fail; }

   %set_output(o);

#else

   %set_output(SWIG_From_int($1));

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

%apply double *rhpDblOut { double *level, double *dual }

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

// WARNING:: do not change these names
%apply (double * rhpVecOutAvar) { double *vlevel, double *vdual };
%apply (double * rhpVecOutAequ) { double *elevel, double *edual };



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

%typemap(out,noblock=1) struct rhp_aequ *, struct rhp_avar * {
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

%typemap(in,numinputs=0,noblock=1) struct rhp_mdl **mdlout ($*1_ltype temp) {
  $1 = &temp;
}
%typemap(argout,noblock=1) struct rhp_mdl **mdlout {
   int new_flags = (SWIG_POINTER_OWN | %newpointer_flags);
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

/* ----------------------------------------------------------------------
 * START specialized typemaps
 * ---------------------------------------------------------------------- */

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

/* getspecialfloats */
%typemap(in,numinputs=1,noblock=1) (const rhp_mdl_t *mdl, double * minf, double * pinf, double * nan) (void *argp, double minf_, double pinf_, double nan_) {
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

%typemap(argout) (const rhp_mdl_t *mdl, double *minf, double *pinf, double* nan) {
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

%include reshop_objects.i

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

%include generated/pyobj_methods.i

