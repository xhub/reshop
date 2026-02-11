/* This file deals with ReSHOP python objects */

/* Since we use builtin we have to assign slot to functions */
#if defined(SWIGPYTHON_BUILTIN)
/* The next 3 are for the mapping protocol (PyMappingMethods) */
%feature("python:slot", "mp_length", functype="lenfunc") __len__;
%feature("python:slot", "mp_subscript", functype="binaryfunc") __getitem__;
%feature("python:slot", "mp_ass_subscript", functype="objobjargproc") __setitem__;
%feature("python:slot", "sq_contains", functype="objobjproc") __contains__;

%feature("python:slot", "tp_str", functype="reprfunc") __str__;
%feature("python:slot", "nb_int", functype="unaryfunc") __nb_int__;

%feature("python:tp_iter") rhp_avar "rhp_avar_iter";
%feature("python:tp_iter") rhp_aequ "rhp_aequ_iter";

%ignore RhpBackendType::backend;

%immutable;
%include "python_structs.h"

%runtime %{
#define SWIG_NewPointerObj_(ptr, type, flags) SWIG_InternalNewPointerObj(ptr, type, flags)
%}

#else

%runtime %{
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

%constant RhpBackendType BackendGamsGmo = BackendGamsGmoObj;
%constant RhpBackendType BackendReSHOP  = BackendReSHOPObj;
%constant RhpBackendType BackendJulia   = BackendJuliaObj;
%constant RhpBackendType BackendAmpl    = BackendAmplObj;
%constant RhpBackendType BackendInvalid = BackendInvalidObj;

%extend BasisStatus {
   long __nb_int__() {
      return $self->status;
   }

  const char *__str__() {
     return rhp_basis_str($self->status);
  }

   def_pretty_print()
};

%extend RhpBackendType {
   long __nb_int__() {
      return $self->backend;
   }

  const char *__str__() {
     return rhp_backend_str($self->backend);
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
%feature("docstring") rhp_avar "Variable container";

typedef struct rhp_avar {
   %extend {
  rhp_avar() { return rhp_avar_new(); }
  rhp_avar(unsigned size, unsigned start) { return rhp_avar_newcompact(size, start); }
  ~rhp_avar() { rhp_avar_free($self); }

  size_t __len__() const { return rhp_avar_size($self); }

  const char *__str__() {
     char *str;
     IO_PRINT_SWIG(asprintf(&str, "Abstract variable of type '%s' and size %u", rhp_avar_gettypename($self), rhp_avar_size($self)));
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
%feature("docstring") rhp_aequ "Equation container";
typedef struct rhp_aequ {
   %extend {
  rhp_aequ() { return rhp_aequ_new(); }
  rhp_aequ(unsigned size, unsigned start) { return rhp_aequ_newcompact(size, start); }
  ~rhp_aequ() { rhp_aequ_free($self); }

  size_t __len__() const { return rhp_aequ_size($self); }

  const char *__str__() {
     char *str;
     IO_PRINT_SWIG(asprintf(&str, "Abstract equation of type '%s' and size %u", rhp_aequ_gettypename($self), rhp_aequ_size($self)));
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

%newobject rhp_linear_equation::__str__;
%feature("docstring") rhp_linear_equation "Linear equation";
%extend struct rhp_linear_equation {
  rhp_linear_equation(struct rhp_mdl *mdl, unsigned size, rhp_idx *vis, double *vals) {
     LinearEquation * lin_equ = PyMem_RawMalloc(sizeof(LinearEquation));
     if (!lin_equ) { SWIG_exception_fail(SWIG_MemoryError, "malloc failed"); }
     lin_equ->mdl = mdl;
     lin_equ->len = size;
     lin_equ->vis = vis;
     lin_equ->vals = vals;
     return lin_equ;
     fail: return NULL;
     }
  ~rhp_linear_equation() { ; }

  size_t __len__() const { return $self->len; }

  const char *__str__() {
     char *str;
     IO_PRINT_SWIG(asprintf(&str, "Linear Equation of size %u", $self->len));
     return str;
     fail:
     return NULL;
  }

   def_pretty_print_alloc()

  PyObject * __getitem__(size_t i) const {
     if (i > $self->len) {
       SWIG_exception(SWIG_IndexError, "Index out of bounds");
     }
     VariableRef * vref = PyMem_RawMalloc(sizeof(VariableRef));
      if (!vref) { SWIG_exception_fail(SWIG_MemoryError, "malloc failed"); }

      vref->mdl = $self->mdl;
      vref->vi = $self->vis[i];

      PyObject *vref_py = SWIG_NewPointerObj_(SWIG_as_voidptr(vref),$descriptor(VariableRef *), SWIG_POINTER_OWN | %newpointer_flags);

      return PyTuple_Pack(2, vref, PyFloat_FromDouble($self->vals[i]));
      fail: return NULL;
   }

  short __contains__(rhp_idx vi) const {
     rhp_idx *vis = $self->vis;
     for (unsigned i = 0, len = $self->len; i < len; ++i) {
        if (vi == vis[i]) { return 1; }
     }

     return 0;
  }
/*  const rhp_idx py__setitem__(size_t i) const { */

};

%handle_exception(rhp_mdl::solve);

%newobject rhp_mdl::__str__;
%newobject rhp_mdl_vars_get;
%newobject rhp_mdl_empdag_get;
//%newobject rhp_mdl::equs;
%feature("docstring") rhp_mdl "Top-level Model";
%feature("docstring") rhp_mdl::solve "solve(gdxname=None)
--

Solve the model.

Parameters
----------

gdxname : str, optional
    if provided, dump the solution into a gdxfile
";

typedef struct rhp_mdl {
   %extend {
      const struct rhp_avar *vars;
      const struct rhp_empdag *empdag;

      rhp_mdl(unsigned backend) { return rhp_mdl_new(backend); } 

      rhp_mdl(RhpBackendType *backend) { return rhp_mdl_new(backend->backend); } 

      rhp_mdl(const char *gms_controlfile) {
         struct rhp_mdl *mdl;
         int rc = rhp_gms_newfromcntr(gms_controlfile, &mdl);
         if (!mdl) {
            SWIG_exception(SWIG_RuntimeError, "Couldn't create model from control file");
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
        IO_PRINT_SWIG(asprintf(&str, "%s Model named '%s' #%u with %u variables and %u equations",
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
        struct rhp_mdl *solver = rhp_mdl_new(RhpBackendGamsGmo);
        RHP_FAIL(solver ? RHP_OK : Error_NullPointer, "Couldn't create the solver model object");
        RHP_FAIL(rhp_process($self, solver), "Couldn't process the model");
        RHP_FAIL(rhp_solve(solver), "Couldn't solve the model");
        RHP_FAIL(rhp_postprocess(solver), "Couldn't report the solution");
        RHP_FAIL(rhp_gms_writesol2gdx($self, gdxname), "Couldn't write the solution as gdx");
        return;
      fail: rhp_exception_code = -1;
     }

     void solve() {
        struct rhp_mdl *solver = rhp_mdl_new(RhpBackendGamsGmo);
        RHP_FAIL(solver ? RHP_OK : Error_NullPointer, "Couldn't create the solver model object");
        RHP_FAIL(rhp_process($self, solver), "Couldn't process the model"); 
        RHP_FAIL(rhp_solve(solver), "Couldn't solve the model");
        RHP_FAIL(rhp_postprocess(solver), "Couldn't report the solution");
        return;
      fail: rhp_exception_code = -1;
     }


     PyObject* draw_empdag() {
        PyObject *tmpfile = NULL, *pydot_mod = NULL, *tempfile_mod = NULL, *py_fname = NULL, *graphs = NULL;
        const char *fname = NULL;
        FILE *f = NULL;

        pydot_mod = PyImport_ImportModule("pydot");
        if (!pydot_mod) {
           PyErr_SetString(PyExc_RuntimeError, "Could not import pydot. Check that it is installed in your environment");
           goto fail;
        }
        tempfile_mod = PyImport_ImportModule("tempfile");
        if (!tempfile_mod) {
           PyErr_SetString(PyExc_RuntimeError, "Could not import the tempfile");
           goto fail;
        }
        tmpfile = PyObject_CallMethodObjArgs(tempfile_mod, PyString_FromString("NamedTemporaryFile"), NULL);
        if (!tmpfile) {
           PyErr_SetString(PyExc_RuntimeError, "Could not create a temporary file");
           goto fail;
        }

        py_fname = PyObject_GetAttrString(tmpfile, "name");
        if (!py_fname) {
           PyErr_SetString(PyExc_RuntimeError, "Could not get the name of the temporary file");
           goto fail;
        }

        fname = PyUnicode_AsUTF8(py_fname);
        if (!fname) {
           goto fail;
        }

        f = fopen(fname, "w");
        if (!f) {
           char buf[256], msg[512], msg_[] = "Could not open the temporary file. Error message is: ";
           const char *dummy_ptr;
           memcpy(msg, msg_, sizeof(msg_));
           SYS_ERRMSG(sizeof(buf)-1, buf, &dummy_ptr);
           strncat(msg, buf, sizeof(msg) - sizeof(msg_) - 1);

           PyErr_SetString(PyExc_RuntimeError, msg);
           goto fail;
        }

        RHP_FAIL(rhp_empdag_writeDOT($self, f), "Couldn't write the DOT representation");

        fclose(f);

        graphs = PyObject_CallMethodObjArgs(pydot_mod, PyString_FromString("graph_from_dot_file"), py_fname, NULL);
        if (!graphs) {
           goto fail;
        }

        PyObject_CallMethodObjArgs(tmpfile, PyString_FromString("close"), NULL);

        Py_DECREF(py_fname);
        Py_DECREF(tmpfile);
        Py_DECREF(tempfile_mod);
        Py_DECREF(pydot_mod);

        return graphs;

      fail: rhp_exception_code = -1;
      if (tmpfile) {
         PyObject_CallMethodObjArgs(tmpfile, PyString_FromString("close"), NULL);
      }
      Py_XDECREF(py_fname);
      Py_XDECREF(tmpfile);
      Py_XDECREF(pydot_mod);
      Py_XDECREF(tempfile_mod);

      return NULL;
     }

  }
} Model;

%{
struct rhp_avar* rhp_mdl_vars_get(struct rhp_mdl *mdl) {
      return rhp_avar_newcompact(rhp_mdl_nvars(mdl), 0);
}

struct rhp_empdag* rhp_mdl_empdag_get(struct rhp_mdl *mdl) {
        EmpDag *empdag = PyMem_RawMalloc(sizeof(EmpDag));
        if (!empdag) {
           PyErr_SetString(PyExc_MemoryError, "Could not create EmpDag object");
           return NULL;
        }

        empdag->mdl = mdl;

        return empdag;
     }
%}

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

VAR_GETSET(level, value);
VAR_GETSET(dual, multiplier);

EQU_GETSET(level, value);
EQU_GETSET(dual, multiplier);

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
   int basis_info;
   CHK_RET(rhp_mdl_getvarbasis(o->mdl, o->vi, &basis_info));
   return basisstatus_getobj(basis_info);

   fail: return NULL;
}
static void var_setbasis(struct rhp_variable_ref *o, BasisStatus *basis) {
   CHK_RET_EXCEPTION(rhp_mdl_setvarbasis(o->mdl, o->vi, basis->status));
}

static PyObject* equ_getbasis(struct rhp_equation_ref *o) {
   int basis_info;
   CHK_RET(rhp_mdl_getequbasis(o->mdl, o->ei, &basis_info));
   return basisstatus_getobj(basis_info);

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
         IO_PRINT_SWIG(asprintf(&str, FMT_COMMON "Objvar is %s; Objequ is %s",
                          (int)strlen(name), name, id, sensestr, nvars, ncons,
                          rhp_mdl_printvarname(mdl, objvar),
                          rhp_mdl_printequname(mdl, objequ)));
         break;
      }
      case RHP_FEAS: {
         unsigned nvifunc = rhp_mp_nmatched($self);
         IO_PRINT_SWIG(asprintf(&str, FMT_COMMON "%u matched variables and equations\n",
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

// RHP_PUBLIB rhp_idx rhp_mp_getobjequ(const rhp_mathprgm_t *mp);
// RHP_PUBLIB rhp_idx rhp_mp_getobjvar(const rhp_mathprgm_t *mp);
// RHP_PUBLIB unsigned rhp_mp_getsense(const rhp_mathprgm_t *mp);
// RHP_PUBLIB unsigned rhp_mp_ncons(const rhp_mathprgm_t *mp);
// RHP_PUBLIB unsigned rhp_mp_nmatched(const rhp_mathprgm_t *mp);
// RHP_PUBLIB unsigned rhp_mp_nvars(const rhp_mathprgm_t *mp);

%attribute_readonly(rhp_mathprgm, const char*, name, getname, rhp_mp_getname(self_));
%attribute_readonly(rhp_mathprgm, unsigned, id, getid, rhp_mp_getid(self_));

// Note to self: since struct rhp_nash_equilibrium is opaque, one needs to "declare" the struct there

%newobject rhp_nash_equilibrium::__str__;
%ignore rhp_nash_equilibrium::mdl;
typedef struct rhp_nash_equilibrium {
%extend  {

   const char *__str__() {
      char *str;

      unsigned id = rhp_nash_getid($self);
      unsigned nchildren = rhp_nash_getnumchildren($self);
      const char *name = rhp_nash_getname($self);

      IO_PRINT_SWIG(asprintf(&str, "Nash Equilibrium '%.*s' (ID #%u) with %u children.",
                          (int)(name ? strlen(name) : 6), name ? name : "NONAME", id, nchildren))
      return str;
      fail: return NULL;
   }

   def_pretty_print_alloc()
}

} NashEquilibrium;

%attribute_readonly(rhp_nash_equilibrium, const char*, name, getname, rhp_nash_getname(self_));
%attribute_readonly(rhp_nash_equilibrium, unsigned, id, getid, rhp_nash_getid(self_));

/* ----------------------------------------------------------------------
 * EMPDAG
 * ---------------------------------------------------------------------- */

%newobject rhp_tuple_empnodes::__str__;

%extend struct rhp_tuple_empnodes {

   const char *__str__() {
      char *str;
      const rhp_mdl_t *mdl = $self->mdl;

      IO_PRINT_SWIG(asprintf(&str,
            "Tuple of %u EMPDAG nodes with %u MPs and %u Nash for %s Model named '%s' #%u",
            rhp_empdag_getnnodes(mdl), rhp_empdag_getnmps(mdl), rhp_empdag_getnnashs(mdl),
            rhp_mdl_getbackendname(mdl), rhp_mdl_getname(mdl), rhp_mdl_getid(mdl))
      );

      return str;
      fail: return NULL;

    }

    int __len__() {
        const rhp_mdl_t *mdl = $self->mdl;
        return rhp_empdag_getnnodes(mdl);
    }

    // This maps to Python's obj[i]
    PyObject * __getitem__(int i) {
        const rhp_mdl_t *mdl = $self->mdl;
        unsigned sz = rhp_empdag_getnnodes(mdl);
        if (sz == UINT_MAX) {
           PyErr_SetString(PyExc_RuntimeError, "Couldn't get the number of nodes from the Model");
           return NULL;
        }

        if (i < 0 || i >= sz) {
            PyErr_SetString(PyExc_IndexError, "Index out of range");
            return NULL; 
        }

        unsigned nmps = rhp_empdag_getnnodes(mdl);
        if (sz == UINT_MAX) {
           PyErr_SetString(PyExc_RuntimeError, "Couldn't get the number of nodes from the Model");
           return NULL;
        }

        if (i < nmps) {
             rhp_mathprgm_t *mp = rhp_empdag_getmp(mdl, i);
             if (!mp) {
                PyErr_SetString(PyExc_RuntimeError, "Couldn't get the MathPrgm");
                return NULL;
             }
             PyObject *o = SWIG_NewPointerObj_(SWIG_as_voidptr(mp),$descriptor(MathPrgm *), %newpointer_flags);
             return o;

        } else {
           rhp_nash_equilibrium_t *nash = rhp_empdag_getnash(mdl, i);
           if (!nash) {
              PyErr_SetString(PyExc_RuntimeError, "Couldn't get the Nash Equilibrium");
              return NULL;
           }
           PyObject *o = SWIG_NewPointerObj_(SWIG_as_voidptr(nash),$descriptor(NashEquilibrium *), %newpointer_flags);
           return o;
        }
    }

   def_pretty_print_alloc()
}

// Note to self: since struct rhp_empdag is opaque, one needs to "declare" the struct there

%newobject rhp_empdag::__str__;
%newobject rhp_empdag_mps_get;
%newobject rhp_empdag_nashs_get;
%newobject rhp_empdag_nodes_get;
%immutable rhp_empdag::mps;
%immutable rhp_empdag::nashs;
%immutable rhp_empdag::nodes;
%ignore rhp_empdag::mdl;
%extend  struct rhp_empdag {

   EmpMathPrgmTuple *mps;
   EmpNashEquilibriumTuple *nashs;
   EmpNodesTuple *nodes;

   const char *__str__() {
      char *str;

      const rhp_mdl_t *mdl = $self->mdl;
      unsigned mdlid = rhp_mdl_getid(mdl);
      const char *mdlname = rhp_mdl_getname(mdl);
      const char *mdlbackendname = rhp_mdl_getbackendname(mdl);

      unsigned narcs = rhp_empdag_getnarcs(mdl);
      unsigned nmps = rhp_empdag_getnmps(mdl);
      unsigned nnashs = rhp_empdag_getnnashs(mdl);

      IO_PRINT_SWIG(asprintf(&str, "EmpDag for %s model '%s' (#%u) with %u MPs, %u Nashs, %u arcs.",
                             mdlbackendname, mdlname, mdlid, nmps, nnashs, narcs));
      return str;
      fail: return NULL;
   }

   def_pretty_print_alloc()


}

%{
   EmpMathPrgmTuple * rhp_empdag_mps_get(struct rhp_empdag *empdag) {
      const rhp_mdl_t *mdl = empdag->mdl;
      EmpMathPrgmTuple *mps = PyMem_New(EmpMathPrgmTuple, 1);
      mps->mdl = mdl;

      return mps;
   }

   EmpNashEquilibriumTuple * rhp_empdag_nashs_get(struct rhp_empdag *empdag) {
      const rhp_mdl_t *mdl = empdag->mdl;
      EmpNashEquilibriumTuple *nashs = PyMem_New(EmpNashEquilibriumTuple, 1);
      nashs->mdl = mdl;

      return nashs;
   }

   EmpNodesTuple * rhp_empdag_nodes_get(struct rhp_empdag *empdag) {
      const rhp_mdl_t *mdl = empdag->mdl;
      EmpNodesTuple *nodes = PyMem_New(EmpNodesTuple, 1);
      nodes->mdl = mdl;

      return nodes;
   }
%}

// %(rhp_empdag, BasisStatus, basis, equ_getbasis, equ_setbasis, equ_getbasis(self_), equ_setbasis(self_, val_));
