// need a PySequence_Check
#define GET_INTS(OBJ,INDEX,VAR)                                          \
  PyObject *_TEMP##VAR = PySequence_GetItem(OBJ,INDEX);                 \
  if (!PyInt_Check(_TEMP##VAR))                                         \
  {                                                                     \
    Py_XDECREF(_TEMP##VAR);                                             \
    PyErr_SetString(PyExc_RuntimeError, "expecting an int for " # VAR);       \
    PyObject_Print(OBJ, stderr, 0);                                     \
    return 0;                                                           \
  }                                                                     \
  VAR = PyInt_AsLong(_TEMP##VAR);                                   \
  Py_DECREF(_TEMP##VAR)

%define %SAFE_CAST_INT(pyvar, len, dest_array, array_pyvar, indvar, alloc)
{
    int array_pyvartype_ = PyArray_TYPE((PyArrayObject *)pyvar);
    switch (array_pyvartype_)
    {
      case NPY_INT32:
      {
        array_pyvar = obj_to_array_allow_conversion(pyvar, NPY_INT32, indvar);
        if (!array_pyvar) { PyErr_SetString(PyExc_RuntimeError, "Could not get array for variable" #pyvar); PyObject_Print(pyvar, stderr, 0); return 0; }

%#ifdef _RESHOP_INT_64
        PyErr_Warn(PyExc_UserWarning, "Performance warning: the vector of indices or pointers is in int32, but reshop has 64-bits integers: we have to perform a conversion. Consider given sparse matrix in the right format");
        dest_array = (rhp_idx*) malloc(len * sizeof(rhp_idx));
        if(!dest_array) { PyErr_SetString(PyExc_RuntimeError, "Allocation of i or p failed (triggered by conversion to int32)"); return 0; }

        for(unsigned i = 0; i < len; ++i)
        {
          dest_array[i] = ((int32_t *) array_data(array_pyvar)) [i];
        }
        if (*indvar) Py_DECREF(array_pyvar);
        *indvar = 0;
        alloc = true;
%#else
         (void)alloc;
        dest_array = (rhp_idx *) array_data(array_pyvar);
%#endif
        break;
      }
      case NPY_INT64:
      {
        array_pyvar = obj_to_array_allow_conversion(pyvar, NPY_INT64, indvar);
        if (!array_pyvar) { PyErr_SetString(PyExc_RuntimeError, "Could not get array for variable " #pyvar);  PyObject_Print(pyvar, stderr, 0); return 0; }

%#ifdef _RESHOP_INT_64
        dest_array = (rhp_idx*) array_data(array_pyvar);
%#else
        PyErr_Warn(PyExc_UserWarning, "Performance warning: the vector of indices or pointers is in int64, but reshop has 32-bits integers: we have to perform a conversion. Consider given sparse matrix in the right format");
        dest_array = (rhp_idx*) malloc(len * sizeof(rhp_idx));
        if(!dest_array) { PyErr_SetString(PyExc_RuntimeError, "Allocation of i or p failed (triggered by conversion to int64)"); return 0; }
        for(unsigned i = 0; i < len; ++i)
        {
          dest_array[i] = ((int64_t *) array_data(array_pyvar)) [i];
        }
        if (*indvar) Py_DECREF(array_pyvar);
        *indvar = 0;
        alloc = true;
%#endif
        break;
      }
      default:
      {
        PyObject *errmsg;
        errmsg = PyUString_FromString("Unknown type ");
        PyUString_ConcatAndDel(&errmsg, PyObject_Repr((PyObject *)PyArray_DESCR((PyArrayObject *)pyvar)));
        PyUString_ConcatAndDel(&errmsg, PyUString_FromFormat(" for variable " #pyvar));
        PyErr_SetObject(PyExc_TypeError, errmsg);
        Py_DECREF(errmsg);
        return 0;
      }
    }
}
%enddef

%fragment("ReSHOPMatrix", "header", fragment="NumPy_Fragments")
{
  static int convert_from_scipy_sparse(PyObject* obj, size_t *nnz, int **ip, int **p, double **x, PyArrayObject** array_data_, int* array_data_ctrl_, PyArrayObject** array_i_, int* array_i_ctrl_, PyArrayObject** array_p_, int* array_p_ctrl_, int* alloc_ctrl)
  {

  assert(obj);
  /* get sys.modules dict */
  PyObject* sys_mod_dict = PyImport_GetModuleDict();
  /* get the scipy module object */ 
  PyObject* scipy_mod = PyMapping_GetItemString(sys_mod_dict, (char *)"scipy.sparse");

  if (!scipy_mod) { 
    PyErr_SetString(PyExc_RuntimeError, "Did you import scipy.sparse ?");
    return 0;
  }

    PyObject* shape_ = PyObject_GetAttrString(obj, "shape");

    UNUSED unsigned nrows = 0, ncols = 0;
    if (shape_) {
      GET_INTS(shape_, 0, nrows);
      GET_INTS(shape_, 1, ncols);

      Py_DECREF(shape_);
    }


    PyObject *res, *coo;
    res = PyObject_CallMethodObjArgs(scipy_mod, PyString_FromString("isspmatrix_coo"), obj, NULL);
    bool is_coo = (res == Py_True);
    Py_DECREF(res);

    int coo_new_alloc;
    if (!is_coo) {
      /* PyErr_Warn(PyExc_UserWarning, "Performance warning: the given sparse matrix is not COO, we have to perform a conversion to COO"); */
      coo = PyObject_CallMethodObjArgs(scipy_mod, PyString_FromString("coo_matrix"), obj, NULL);
      if (!coo) { if (!PyErr_Occurred()) { PyErr_SetString(PyExc_RuntimeError, "Conversion to coo failed!"); }; return 0; }
      coo_new_alloc = 1;
    } else {
      coo = obj;
      coo_new_alloc = 0;
    }

    // Get nnz
    PyObject* nnz_ = PyObject_GetAttrString(coo, "nnz");
    *nnz = PyInt_AsLong(nnz_);
    Py_DECREF(nnz_);


    PyObject* data_ = PyObject_GetAttrString(coo, "data");
    PyObject* row_ = PyObject_GetAttrString(coo, "row");
    PyObject* col_ = PyObject_GetAttrString(coo, "col");

    *array_data_ = obj_to_array_allow_conversion(data_, NPY_DOUBLE, array_data_ctrl_);
    if (!*array_data_) { PyErr_SetString(PyExc_RuntimeError, "Could not get a pointer to the data array");  PyObject_Print(data_, stderr, 0); return 0; }

    *x = (double*)array_data(*array_data_);

    bool alloc_p = false;
    %SAFE_CAST_INT(col_, *nnz, (*p), *array_p_, array_p_ctrl_, alloc_p);
    bool alloc_i = false;
    %SAFE_CAST_INT(row_, *nnz, (*ip), *array_i_, array_i_ctrl_, alloc_i);

    if (coo_new_alloc) {
      Py_DECREF(coo);
    }

    return 1;
   }
}

%typemap(in, fragment="ReSHOPMatrix") (size_t nnz, unsigned *i, unsigned *j, double *x)
   (int array_data_ctrl_ = 0,
   int array_i_ctrl_ = 0,
   int array_p_ctrl_ = 0,
   SN_ARRAY_TYPE *array_data_ = NULL,
   SN_ARRAY_TYPE *array_i_ = NULL,
   SN_ARRAY_TYPE *array_p_ = NULL,
   int alloc_ctrl_ = 0,
   size_t nnz = 0,
   rhp_idx *i = NULL,
   rhp_idx *j = NULL,
   double *x = NULL)
{
#ifdef SWIGPYTHON
  int res = convert_from_scipy_sparse($input, &nnz, &i, &j, &x, &array_data_, &array_data_ctrl_, &array_i_,
  &array_i_ctrl_, &array_p_, & array_p_ctrl_, &alloc_ctrl_);
#endif /* SWIGPYTHON */

#ifdef SWIGMATLAB
  int res = cs_convert_from_matlab($input, &nnz, &i, &j, &x, &array_data_, &array_data_ctrl_, &array_i_, &array_i_ctrl_, &array_p_, &array_p_ctrl_, &alloc_ctrl_);
#endif /* SWIGMATLAB */

  if (!res) { SWIG_fail; }

  $1 = nnz;
  $2 = i;
  $3 = j;
  $4 = x;
}

%typemap(freearg) (size_t nnz, unsigned *i, unsigned *j, double *x)
{
  target_mem_mgmt(array_data_ctrl_$argnum,  array_data_$argnum);
  target_mem_mgmt(array_i_ctrl_$argnum,  array_i_$argnum);
  target_mem_mgmt(array_p_ctrl_$argnum,  array_p_$argnum);
}

%typecheck(SWIG_TYPECHECK_SIZE) (size_t nnz) {
  /* get sys.modules dict */
  PyObject* sys_mod_dict = PyImport_GetModuleDict();
  /* get the scipy module object */ 
  PyObject* scipy_mod = PyMapping_GetItemString(sys_mod_dict, (char *)"scipy.sparse");

  if (!scipy_mod) { 
    PyErr_SetString(PyExc_RuntimeError, "Did you import scipy.sparse ?");
    return 0;
  }

  $1 = (is_array($input) || PySequence_Check($input) || is_Pyobject_scipy_sparse_matrix($input, scipy_mod));
}
