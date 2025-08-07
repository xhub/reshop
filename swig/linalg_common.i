#ifdef SWIGPYTHON

%ignore is_Pyobject_scipy_sparse_matrix;

%inline %{

static inline bool is_Pyobject_scipy_sparse_matrix(PyObject* o, PyObject* scipy_mod)
 {
    bool ret;
    PyObject* res = PyObject_CallMethodObjArgs(scipy_mod, PyString_FromString("issparse"), o, NULL);

    if (!res) return false;

    ret = (res == Py_True);
    Py_DECREF(res);

    return ret;
}


#define CHECK_ARRAY(X) \
!require_native(X)

#define CHECK_ARRAY_VECTOR(X) \
CHECK_ARRAY(X) || !require_contiguous(X) || !require_dimensions(X, 1)

#define CHECK_ARRAY_MATRIX(X) \
CHECK_ARRAY(X) || !require_fortran(X) || !require_dimensions(X, 2)

#define CHECK_ARRAY_SIZE(req_size, array, indx) (req_size == array_size(array, indx))

#define obj_to_sn_array(obj, alloc) obj_to_array_fortran_allow_conversion(obj, NPY_DOUBLE, alloc);
#define obj_to_sn_vector(obj, alloc) obj_to_array_contiguous_allow_conversion(obj, NPY_DOUBLE, alloc);

#define create_target_vec(size, array) \
   npy_intp this_vector_dim[1] = { size }; \
   array = (PyArrayObject *)PyArray_SimpleNew(1, this_vector_dim, NPY_DOUBLE);
%}

#endif /* SWIGPYTHON */

#ifdef SWIGMATLAB
%inline %{

static inline long int array_size(mxArray* m, int indx)
{
  const mwSize *dim_array;
  dim_array = mxGetDimensions(m);
  return (long int)dim_array[indx];
}

#define array_numdims(X) (int)mxGetNumberOfDimensions(X)

#define CHECK_ARRAY(X) false

#define CHECK_ARRAY_VECTOR(X) !(array_numdims(X) == 2 && ((array_size(X, 0) == 1 && (array_size(X, 1) > 0)) || (array_size(X, 0) > 0 && (array_size(X, 1) == 1))))

#define CHECK_ARRAY_MATRIX(X) !(array_numdims(X) == 2 && array_size(X, 0) > 0 && array_size(X, 1) > 0)

#define CHECK_ARRAY_SIZE(req_size, array, indx) (req_size == array_size(array, indx))

#define CHECK_ARRAY_SIZE_VECTOR(req_size, array) CHECK_ARRAY_SIZE(req_size, array, 0) || CHECK_ARRAY_SIZE(req_size, array, 1)

#define array_data(X) mxGetData(X)

// XXX maybe we need to copy stuff here? -- xhub
#define obj_to_sn_array(obj, dummy) (mxIsDouble(obj)) ? obj : NULL
#define obj_to_sn_vector(obj, dummy) (mxIsDouble(obj) && !mxIsSparse(obj)) ? obj : NULL
#define obj_to_sn_vector_int(obj, dummy) sizeof(int) == 8 ? (mxIsInt32(obj) ? obj : NULL) : (mxIsInt64(obj) ? obj : NULL)


%}
#endif /* SWIGMATLAB */


%typemap(in,fragment="NumPy_Fragments") (const struct rhp_mdl *mdl, double *) (SN_ARRAY_TYPE* array = NULL) {
   int res = SWIG_ConvertPtr($input, (void**)&$1, $1_descriptor, %convertptr_flags);
   if (!SWIG_IsOK(res)) {
     SWIG_exception_fail(SWIG_ArgError(res), "in method '" "$symname" "', argument " "$argnum" " of type '" "$1_type" "'");
   }

   unsigned nvars = rhp_mdl_nvars($1);
   unsigned nequs = rhp_mdl_nequs($1);
   size_t vec_size = MAX(nvars, nequs);
   create_target_vec(vec_size, array);

   $2 = (double *) array_data(array);
}

%typemap(argout) (const struct rhp_mdl *mdl, double *) {
    %append_output((PyObject *)array$argnum);
    /* TODO: Py_DECREF ? */
}

%typemap(in,fragment="NumPy_Fragments") (struct rhp_avar *v, double * OUTPUT) (SN_ARRAY_TYPE* array = NULL) {
   int res = SWIG_ConvertPtr($input, (void**)&$1, $1_descriptor, %convertptr_flags);
   if (!SWIG_IsOK(res)) {
     SWIG_exception_fail(SWIG_ArgError(res), "in method '" "$symname" "', argument " "$argnum" " of type '" "$1_type" "'");
   }

   size_t vec_size = rhp_avar_size($1);
   create_target_vec(vec_size, array);

   $2 = (double *) array_data(array);
}

%typemap(argout) (struct rhp_avar *v, double * OUTPUT) {
    %append_output((PyObject *)array$argnum);
    /* TODO: Py_DECREF ? */
}

%typemap(in,fragment="NumPy_Fragments") (struct rhp_aequ *e, double * OUTPUT) (SN_ARRAY_TYPE* array = NULL) {
   int res = SWIG_ConvertPtr($input, (void**)&$1, $1_descriptor, %convertptr_flags);
   if (!SWIG_IsOK(res)) {
     SWIG_exception_fail(SWIG_ArgError(res), "in method '" "$symname" "', argument " "$argnum" " of type '" "$1_type" "'");
   }

   size_t vec_size = rhp_aequ_size($1);
   create_target_vec(vec_size, array);

   $2 = (double *) array_data(array);
}

%typemap(argout) (struct rhp_aequ *e, double * OUTPUT) {
    %append_output((PyObject *)array$argnum);
    /* TODO: Py_DECREF ? */
}


/* TODO(URG) the arguments should be packed in a tuple, or a check with some other mechanism */
%typemap(in,fragment="NumPy_Fragments") (double *rhp_dblin) (SN_ARRAY_TYPE* array = NULL, int is_new_object = 0) {
  array = obj_to_sn_vector($input, &is_new_object);

  if (!array) {
   SWIG_exception_fail(SWIG_TypeError, "Could not get a SN_ARRAY_TYPE from the given object");
  }

  if (CHECK_ARRAY_VECTOR(array)) {
   SWIG_exception_fail(SWIG_TypeError, "The given object does not have the right structure. We expect a vector (or list, tuple, ...)");
  }

  $1 = (double *) array_data(array);
}
