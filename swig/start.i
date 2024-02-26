// signatures
// docstrings are generated from doxygen thanks to an external tool
// (doxy2swig.py)
//%feature("autodoc", 0);

// named parameters (broken with overloaded function)
// %feature("kwargs");

// Do not create default constructor
%nodefaultctor;


// to avoid name conflicts
%{
#define SWIG_FILE_WITH_INIT

#ifdef __GNUC__
_Pragma("GCC diagnostic error \"-Wimplicit-function-declaration\"")
_Pragma("GCC diagnostic ignored \"-Wunused-parameter\"")
#define UNUSED __attribute__((unused))

#elif __clang__

_Pragma("clang diagnostic error \"-Wimplicit-function-declaration\"")
_Pragma("clang diagnostic ignored \"-Wunused-parameter\"")
_Pragma("clang diagnostic ignored \"-Wunused-function\"")
#define UNUSED __attribute__((unused))


#else

#define UNUSED 

#endif
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
