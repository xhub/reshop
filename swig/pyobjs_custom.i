%runtime %{
/* custome tp_iter for the iterator */
    SWIGINTERN PyObject* AequvarIter_iter(PyObject *self) {
        Py_INCREF(self);
        return self;
    }

   SWIGINTERN PyObject* AequvarIter_str(PyObject *self);

// Custom C struct for the iterator object
typedef struct {
    PyObject_HEAD
    PyObject *container;
    struct rhp_avar *avar; // A reference to the container object we're iterating over
    unsigned idx;  // The current position in the iteration
    unsigned sz;  // The current position in the iteration
} AvarIterObject;

// Deallocator for the iterator object
static void AvarIter_dealloc(AvarIterObject *self) {
    Py_XDECREF(self->container);
    Py_TYPE(self)->tp_free((PyObject *)self);
}

// __next__ method for the iterator. Returns the next item.
static PyObject *AvarIter_next(PyObject *self) {
    AvarIterObject *it = (AvarIterObject *)self;
    struct rhp_avar *avar = it->avar;

    if (!avar) {
       PyErr_SetString(PyExc_RuntimeError, "No abstract variable in iterator!");
       return NULL;
    }

    // Check if we've reached the end of the data
    if (it->idx >= it->sz) {
        PyErr_SetNone(PyExc_StopIteration); // Set the StopIteration exception
        it->idx = 0; // Reset for next iteration of the container
        return NULL; // Return NULL to signal the end
    }

    // Get the value and create a new Python object
    rhp_idx value;
    int rc = rhp_avar_get(it->avar, it->idx++, &value);
    if (rc != RHP_OK) {
       PyErr_SetString(PyExc_RuntimeError, rhp_status_descr(rc));
       return NULL;
    }

     return PyInt_FromLong(value);
}

// AvarIter Type Definition
static PyTypeObject AvarIter_Type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "AvarIter",
    .tp_doc = "Iterator for Avar objects",
    .tp_basicsize = sizeof(AvarIterObject),
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_new = PyType_GenericNew,
    .tp_dealloc = (destructor)AvarIter_dealloc,
    .tp_str = (reprfunc)AequvarIter_str,
    .tp_iter = (getiterfunc)AequvarIter_iter,       // Defines the iterator interface
    .tp_iternext = (iternextfunc)AvarIter_next, // Defines the next item logic
};

typedef struct {
    PyObject_HEAD
    PyObject *container;
    struct rhp_aequ *aequ; // A reference to the container object we're iterating over
    unsigned idx;  // The current position in the iteration
    unsigned sz;  // The current position in the iteration
} AequIterObject;

// Deallocator for the iterator object
static void AequIter_dealloc(AequIterObject *self) {
    Py_XDECREF(self->container);
    Py_TYPE(self)->tp_free((PyObject *)self);
}

// __next__ method for the iterator. Returns the next item.
static PyObject *AequIter_next(PyObject *self) {
    AequIterObject *it = (AequIterObject *)self;
    struct rhp_aequ *aequ = it->aequ;

    if (!aequ) {
       PyErr_SetString(PyExc_RuntimeError, "No abstract equiable in iterator!");
       return NULL;
    }

    // Check if we've reached the end of the data
    if (it->idx >= it->sz) {
        PyErr_SetNone(PyExc_StopIteration); // Set the StopIteration exception
        it->idx = 0; // Reset for next iteration of the container
        return NULL; // Return NULL to signal the end
    }

    // Get the value and create a new Python object
    rhp_idx value;
    int rc = rhp_aequ_get(it->aequ, it->idx++, &value);
    if (rc != RHP_OK) {
       PyErr_SetString(PyExc_RuntimeError, rhp_status_descr(rc));
       return NULL;
    }

     return PyInt_FromLong(value);
}

// AequIter Type Definition
static PyTypeObject AequIter_Type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "AequIter",
    .tp_doc = "Iterator for Aequ objects",
    .tp_basicsize = sizeof(AequIterObject),
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_new = PyType_GenericNew,
    .tp_dealloc = (destructor)AequIter_dealloc,
    .tp_str = (reprfunc)AequvarIter_str,
    .tp_iter = (getiterfunc)AequvarIter_iter,       // Defines the iterator interface
    .tp_iternext = (iternextfunc)AequIter_next, // Defines the next item logic
};

SWIGINTERN PyObject* AequvarIter_str(PyObject *o) {

      char *type;
      const char *typename;
      size_t sz;
      if (PyObject_TypeCheck(o, &AvarIter_Type)) {
         AvarIterObject *it = (AvarIterObject*)o;
         type = "variable";
         typename = rhp_avar_gettypename(it->avar);
         sz = rhp_avar_size(it->avar);
      } else if (PyObject_TypeCheck(o, &AequIter_Type)) {
         AequIterObject *it = (AequIterObject*)o;
         type = "equation";
         typename = rhp_aequ_gettypename(it->aequ);
         sz = rhp_aequ_size(it->aequ);
      } else {
         return PyUnicode_FromString("ERROR: Invalid iterator type");
      }

      return PyUnicode_FromFormat("Iterator for abstract %s of type '%s' and size %zu",
                                  type, typename, sz);
}
%}

%wrapper %{

   PyObject* rhp_avar_iter(PyObject *o) {
      SwigPyObject *swigobj = (SwigPyObject *)o;

      // Sanity checks
      if (!swigobj->ptr) {
          PyErr_SetString(PyExc_BufferError, "SWIG pointer is NULL.");
          return NULL;
      }

      AvarIterObject *it = (AvarIterObject *)PyObject_CallObject((PyObject *)&AvarIter_Type, NULL);
      if (!it) { return NULL; }

      it->container = o;
      it->avar = swigobj->ptr;
      it->sz = rhp_avar_size(it->avar); 
      it->idx = 0; 

      Py_INCREF(o);

      return (PyObject*)it;
   }

   PyObject* rhp_aequ_iter(PyObject *o) {
      SwigPyObject *swigobj = (SwigPyObject *)o;

      // Sanity checks
      if (!swigobj->ptr) {
          PyErr_SetString(PyExc_BufferError, "SWIG pointer is NULL.");
          return NULL;
      }

      AequIterObject *it = (AequIterObject *)PyObject_CallObject((PyObject *)&AequIter_Type, NULL);
      if (!it) { return NULL; }

      it->container = o;
      it->aequ = swigobj->ptr;
      it->sz = rhp_aequ_size(it->aequ); 
      it->idx = 0; 

      Py_INCREF(o);

      return (PyObject*)it;
   }

%}

%init %{
    // Finalize and register the iterator types
    if (PyType_Ready(&AvarIter_Type) < 0) return NULL;
    if (PyType_Ready(&AequIter_Type) < 0) return NULL;
%}
