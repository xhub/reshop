
if (NOT BUILD_PYTHON)
   return()
endif()

cmake_minimum_required(VERSION 3.22)
find_package(SWIG REQUIRED)
include(UseSWIG)

# linux target for python wheel
if (LINUX AND DISTRIB)
   set(PYTHON_WHEEL_PLAT "manylinux_2_28_x86_64")
endif()


function(clone_imported_target ORIG_TARGET NEW_TARGET)
    # 1. Safety check: ensure the original target actually exists
    if(NOT TARGET ${ORIG_TARGET})
        message(FATAL_ERROR "Cannot clone: Target '${ORIG_TARGET}' does not exist.")
    endif()

    # 2. Extract the target type (e.g., SHARED_LIBRARY, STATIC_LIBRARY)
    get_target_property(TARGET_TYPE ${ORIG_TARGET} TYPE)

    # 3. Map the internal CMake type to the add_library keyword
    if(TARGET_TYPE STREQUAL "SHARED_LIBRARY")
        set(LIB_TYPE SHARED)
    elseif(TARGET_TYPE STREQUAL "STATIC_LIBRARY")
        set(LIB_TYPE STATIC)
    elseif(TARGET_TYPE STREQUAL "UNKNOWN_LIBRARY")
        set(LIB_TYPE UNKNOWN)
    elseif(TARGET_TYPE STREQUAL "INTERFACE_LIBRARY")
        set(LIB_TYPE INTERFACE)
    else()
        message(FATAL_ERROR "Unsupported target type for cloning: ${TARGET_TYPE}")
    endif()

    # 4. Create the brand new, disconnected imported target
    add_library(${NEW_TARGET} ${LIB_TYPE} IMPORTED)

    # 5. Define the list of crucial properties to copy.
    # (You can add more specific properties here if your project needs them)
    set(PROPERTIES_TO_CLONE
        IMPORTED_LOCATION
        IMPORTED_IMPLIB
        IMPORTED_LOCATION_RELEASE
        IMPORTED_LOCATION_DEBUG
        INTERFACE_INCLUDE_DIRECTORIES
        INTERFACE_LINK_LIBRARIES
        INTERFACE_COMPILE_DEFINITIONS
        INTERFACE_COMPILE_OPTIONS
    )

    # 6. Loop through and copy each property if it exists
    foreach(PROP IN LISTS PROPERTIES_TO_CLONE)
        get_target_property(PROP_VALUE ${ORIG_TARGET} ${PROP})
        
        # In CMake, if a property is missing, it returns "<VAR>-NOTFOUND".
        # This standard if() check safely evaluates to false if it was not found.
        if(PROP_VALUE)
            set_target_properties(${NEW_TARGET} PROPERTIES ${PROP} "${PROP_VALUE}")
        endif()
    endforeach()
endfunction()


# We generate the python bindings in 2 steps:
# - 1. Use SWIG to generate the wrapper (just python for now)
# - 2. For each specified python interpreter, compile the module

# This is our hackish way to ensure proper tracking of dependencies of
# included swig ".i" files
file(GLOB_RECURSE SWIG_I_INCLUDES *.i)

set_property(SOURCE reshop.i PROPERTY C ON)
set_property(SOURCE reshop.i PROPERTY SWIG_MODULE_NAME reshop)
set_property(SOURCE reshop.i PROPERTY OUTPUT_DIR ${CMAKE_BINARY_DIR}/swig)
set_property(SOURCE reshop.i PROPERTY USE_SWIG_DEPENDENCIES ON)
set(CMAKE_SWIG_FLAGS -O -Wall -Werror -doxygen -builtin)

# Should not be needed with the above but doesn't hurt
set(SWIG_USE_SWIG_DEPENDENCIES ON)


# swig_add_library could be nice, but it requires a lot of changes ...

# Regenerate all the files:
# 1. Run clang to parse reshop.h
# 2. Run utils/sw

find_program(CLANG_EXECUTABLE clang)

if(CLANG_EXECUTABLE AND BUILD_DOCUMENTATION)
    message(STATUS "Python: Clang command found at: ${CLANG_EXECUTABLE}")

   set(AST_JSON_FILE "${CMAKE_BINARY_DIR}/generated/ast.json")

   add_custom_command(
    OUTPUT  "${AST_JSON_FILE}"
    COMMAND ${CLANG_EXECUTABLE} -Xclang -ast-dump=json -fsyntax-only "${CMAKE_SOURCE_DIR}/src/api/reshop.h" > ${AST_JSON_FILE}
    DEPENDS "${CMAKE_SOURCE_DIR}/src/api/reshop.h"
    COMMENT "Generating JSON AST for reshop.h"
   )

   list(GET BUILD_PYTHON 0 PY_INTERP)

   add_custom_command(
      OUTPUT            "${CMAKE_CURRENT_SOURCE_DIR}/generated/pyobj_methods_docstring.i.in"
                        "${CMAKE_CURRENT_SOURCE_DIR}/generated/pyobj_methods.i"
                        "${CMAKE_CURRENT_SOURCE_DIR}/generated/rename_named.i"
      COMMAND           "${PY_INTERP}" "${CMAKE_SOURCE_DIR}/utils/swig_generate_methods_from_api.py" "${AST_JSON_FILE}"
      DEPENDS           "${AST_JSON_FILE}"
                        "${CMAKE_SOURCE_DIR}/utils/swig_generate_methods_from_api.py"
      WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
      COMMENT           "Generating SWIG methods from API"
   )

   add_custom_command(
      OUTPUT            "${CMAKE_CURRENT_SOURCE_DIR}/generated/pyobj_methods_docstring.i"
                        "${CMAKE_CURRENT_SOURCE_DIR}/generated/reshop_docs.i"
                        "${CMAKE_CURRENT_SOURCE_DIR}/generated/reshop_docs.py"
      COMMAND           "${PY_INTERP}" "${CMAKE_SOURCE_DIR}/utils/doxy2numpydoc.py" "${CMAKE_BINARY_DIR}/docs/xml/index.xml"
      DEPENDS           "${AST_JSON_FILE}"
                        "${CMAKE_CURRENT_SOURCE_DIR}/generated/pyobj_methods_docstring.i.in"
                        "${CMAKE_BINARY_DIR}/docs/xml/index.xml"
                        "${CMAKE_SOURCE_DIR}/utils/doxy2numpydoc.py"
      WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
      ${DOXY2NUMPYDOC_EXTRA}
      COMMENT           "Generating SWIG docstrings from doxygen"
   )

   find_program(NUMPYDOC_EXE numpydoc)

   if (NUMPYDOC_EXE)
      add_custom_command(
         OUTPUT             "${CMAKE_BINARY_DIR}/generated/numpydoc.stdout"
         COMMAND            "${NUMPYDOC_EXE}" lint ${CMAKE_CURRENT_SOURCE_DIR}/generated/reshop_docs.py > "${CMAKE_BINARY_DIR}/generated/numpydoc.stdout"
         WORKING_DIRECTORY  "${CMAKE_CURRENT_SOURCE_DIR}"
         DEPENDS            "${CMAKE_CURRENT_SOURCE_DIR}/generated/reshop_docs.py"
         COMMENT            "Checking docstrings"
      )
      set(DOXY2NUMPYDOC_EXTRA MAIN_DEPENDENCY ${CMAKE_BINARY_DIR}/generated/numpydoc.stdout)
   endif(NUMPYDOC_EXE)

   LIST(APPEND GENERATED_I "${CMAKE_CURRENT_SOURCE_DIR}/generated/pyobj_methods_docstring.i"
                           "${CMAKE_CURRENT_SOURCE_DIR}/generated/reshop_docs.i"
                           "${CMAKE_CURRENT_SOURCE_DIR}/generated/pyobj_methods.i"
                           "${CMAKE_CURRENT_SOURCE_DIR}/generated/rename_named.i")


endif()



# Use internal macros, but that's how it is ...
swig_module_initialize(swig_py_reshop PYTHON)
# Add Custom target to keep track of things
add_custom_target(swig_py_reshop DEPENDS ${LIBRESHOP} ${SWIG_I_INCLUDES} ${GENERATED_I})

## TODO: For some reason, only the last command works. 

#set_property(TARGET ${reshop_py_wrap} PROPERTY SWIG_DEPENDS ${SWIG_I_INCLUDES} ${GENERATED_I})
#set_property(SOURCE reshop.i PROPERTY DEPENDS ${SWIG_I_INCLUDES} ${GENERATED_I})
set_property(SOURCE reshop.i PROPERTY DEPENDS swig_py_reshop)

# Use internal macros, but that's how it is ...
swig_add_source_to_module(swig_py_reshop reshop_py_wrap reshop.i)

# Configure the two files outside of the loop
configure_file( ${CMAKE_CURRENT_SOURCE_DIR}/setup.py.in ${CMAKE_BINARY_DIR}/swig/setup.py.gen @ONLY)
file(GENERATE OUTPUT ${CMAKE_BINARY_DIR}/swig/__init__.py
              CONTENT "__version__ = \"${ReSHOP_VERSION}\"\nfrom .reshop import *")

# If not GAMS install has been detected, rely on the "julia" binaries to get PATH
if (NOT PATH_SOLVER_COPIED)
   download_path_libs()
endif()


macro(SETUP_SWIG_PYTHON_MODULE Py3_arg)

   cmake_path(SET Py3_EXECUTABLE "${Py3_arg}")

   # With GMSPython, we expect the configuration to be stored as a cmake file
   if (${Py3_EXECUTABLE} MATCHES "GMSPython") 
      if (NOT GMSPythonCfg)
         MESSAGE(FATAL_ERROR "When GMSPython is in BUILD_PYTHON, then GMSPythonCfg must be specified" )
      endif()

      include(${GMSPythonCfg})

      if (NOT Python3_EXECUTABLE)
         MESSAGE(FATAL_ERROR "When GMSPython is in BUILD_PYTHON, then Python3_EXECUTABLE must be set to a compatible python
         interpreter" )
      endif()

   else ()

      if(IS_ABSOLUTE Py3_EXECUTABLE)
         cmake_path(SET Python3_EXECUTABLE "${Py3_EXECUTABLE}")
      else()
      # Regular python discovery. We need to force the version to the one
      # specified as argument
         execute_process(COMMAND ${Py3_EXECUTABLE} -c [[import sys; print(f'{sys.executable}')]]
                         OUTPUT_VARIABLE Python3_EXECUTABLE
                         ECHO_OUTPUT_VARIABLE
                         ECHO_ERROR_VARIABLE
                         COMMAND_ECHO STDOUT
                         OUTPUT_STRIP_TRAILING_WHITESPACE)
      endif()

      message(STATUS "For BUILD_PYTHON argument ${Py3_EXECUTABLE}, executable is ${Python3_EXECUTABLE}")

      execute_process (COMMAND ${Python3_EXECUTABLE} -c [[import sys; print(f'{sys.version_info.major}.{sys.version_info.minor}')]]
                       RESULT_VARIABLE PyCmd_VERSION_RC
                       OUTPUT_VARIABLE Py3_VERSION
                       ERROR_VARIABLE  PyCmd_VERSION_STDERR
                       OUTPUT_STRIP_TRAILING_WHITESPACE)

         if (PyCmd_VERSION_RC GREATER 0)
         message(FATAL_ERROR "'{Python3_EXECUTABLE} --version' failed with code ${PyCmd_VERSION_RC}. Stderr content is \n${PyCmd_VERSION_STDERR}\n\n"
                          "Stdout content is \n${Py3_VERSION}\n")
         endif()

      #      separate_arguments (outlist NATIVE_COMMAND ${PyCmd_VERSION})
 
      #message(STATUS "For BUILD_PYTHON argument ${P}; full version is ${PyCmd_VERSION}\nParsed is ${outlist}")
      message(STATUS "For BUILD_PYTHON argument ${Py3_EXECUTABLE}, the version is ${Py3_VERSION}; stderr is ${PyCmd_VERSION_STDERR}")
 
      #LIST(POP_BACK outlist Py3_VERSION)

      # Unset a bunch of stuff to ensure find_package does something ...
      unset(Python3_Found)
      unset(Python3_Interpreter_FOUND)
      #unset(Python3_EXECUTABLE)
      unset(Python3_INTERPRETER)
      unset(Python3_Library)
      unset(Python3_NumPy_FOUND)
      unset(Python3_NumPy_INCLUDE_DIR)
      unset(Python3_NumPy_INCLUDE_DIRS)
      unset(Python3_NumPy_VERSION)
 
      find_package (Python3 REQUIRED
                    COMPONENTS Development.Module NumPy
                    EXACT ${Py3_VERSION})
 
      # Use Python3_add_library()?

      execute_process (COMMAND ${Python3_EXECUTABLE} -c "import sysconfig; print(sysconfig.get_platform())"
                       OUTPUT_VARIABLE Python3_platform
                       OUTPUT_STRIP_TRAILING_WHITESPACE)

   endif()


   message(STATUS "For BUILD_PYTHON argument ${P} with version ${Py3_VERSION}, found include dirs
   ${Python3_INCLUDE_DIRS} for python and ${Python3_NumPy_INCLUDE_DIRS} for numpy")


   set(pytarget _reshop.${Python3_SOABI})

   # asprintf is for compatibility, mostly on windows
   set(reshop_py_sources ${reshop_py_wrap} ${CMAKE_SOURCE_DIR}/src/utils/os/asprintf.c ${CMAKE_SOURCE_DIR}/src/utils/os/asprintf.c)

   add_library(${pytarget} MODULE ${reshop_py_sources})

   # Kill this if Python3_add_library is used
   set_property (TARGET ${pytarget} PROPERTY PREFIX "")
   if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
      set_property (TARGET ${pytarget} PROPERTY SUFFIX ".pyd")
   elseif (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
       set_property (TARGET ${pytarget} PROPERTY SUFFIX ".so") # This looks odd but seems needed
   else()
      set_property (TARGET ${pytarget} PROPERTY SUFFIX "${CMAKE_SHARED_MODULE_SUFFIX}")
   endif()

   set_property(TARGET ${pytarget} APPEND PROPERTY LINK_OPTIONS ${LINKER_FLAGS})
   set_property(TARGET ${pytarget} APPEND PROPERTY LINK_OPTIONS ${Python3_LINK_OPTIONS})

   # Windows needs the python library and MINGW/CYGWIN does not support #pragma
   if(WINDOWS)
      if (${Py3_EXECUTABLE} MATCHES "GMSPython") 
         set(_LIB ${Python3_LIBRARIES})
      else()
         clone_imported_target(Python3::Module ${pytarget}::Module)
         set(_LIB ${pytarget}::Module)
      endif()

      target_link_libraries(${pytarget} PRIVATE ${_LIB} ${RESHOP_LIBNAME})
   else()
      target_link_libraries(${pytarget} PRIVATE ${RESHOP_LIBNAME})
   endif()

   target_include_directories(${pytarget} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
                                                  ${Python3_INCLUDE_DIRS}
                                                  ${Python3_NumPy_INCLUDE_DIRS})

   # Undefined python symbols are okay
   if (APPLE)
      target_link_options(${pytarget} PRIVATE "LINKER:-undefined,dynamic_lookup")
   endif()
   if (LINUX)
      target_link_options(${pytarget} PRIVATE "LINKER:-z,undefs" "LINKER:--allow-shlib-undefined")
   endif()

   # This is necessary for windows ...
   if (Python3_LIBRARY_DIRS)
      target_link_directories(${pytarget} PRIVATE ${Python3_LIBRARY_DIRS})
   endif()

   # Credit for the rest of the function: https://github.com/Mizux/cmake-swig
   #######################
   ## Python Packaging  ##
   #######################
   set(PYTHON_PROJECT_DIR ${CMAKE_BINARY_DIR}/swig/python-${Python3_SOABI})
   file(GENERATE OUTPUT ${PYTHON_PROJECT_DIR}/setup.py INPUT ${CMAKE_BINARY_DIR}/swig/setup.py.gen)
   # Copy the README
   configure_file(${CMAKE_SOURCE_DIR}/README.md ${PYTHON_PROJECT_DIR}/README.md COPYONLY)


   if(VENV_TESTING)
      # Testing using a vitual environment
      set(VENV_EXECUTABLE ${Python3_EXECUTABLE} -m venv)
      set(VENV_DIR ${CMAKE_BINARY_DIR}/venv/${pytarget})

      if(WINDOWS)
         if (CMAKE_CROSSCOMPILING_EMULATOR)
            set(VENV_Python3_EXECUTABLE "${CMAKE_CROSSCOMPILING_EMULATOR};${VENV_DIR}/Scripts/python.exe")
         else()
            set(VENV_Python3_EXECUTABLE "${VENV_DIR}/Scripts/python.exe")
         endif()
      else()
         set(VENV_Python3_EXECUTABLE ${VENV_DIR}/bin/python)
      endif()

      set(TEST_PYTHON_EXECUTABLE ${VENV_Python3_EXECUTABLE})

      # Set working directory to top-level. Avoid loading wrong module
      set(TEST_PYTHON_WRKDIR ${CMAKE_BINARY_DIR})

      if (NOT EXISTS ${VENV_DIR})
         # make a virtualenv to install our python package in it
         execute_process (COMMAND ${VENV_EXECUTABLE} ${VENV_DIR})

         # Make sure pip is installed
         execute_process (COMMAND ${VENV_Python3_EXECUTABLE} -m ensurepip RESULT_VARIABLE ENSUREPIP)

         if (ENSUREPIP GREATER 0)
            file(DOWNLOAD "https://bootstrap.pypa.io/get-pip.py" ${CMAKE_CURRENT_BINARY_DIR}/get-pip.py
           TIMEOUT 10
           STATUS DL_STATUS)

            if(NOT DL_STATUS MATCHES "0;")
               MESSAGE(FATAL_ERROR "ERROR: Couldn't download get-pip.py from URL https://bootstrap.pypa.io/get-pip.py; got status ${DL_STATUS}")
            endif()

            execute_process(COMMAND ${VENV_Python3_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/get-pip.py
                         RESULT_VARIABLE GET_PIP
                         OUTPUT_VARIABLE GETPIP_STDOUT
                         ERROR_VARIABLE  GETPIP_STDERR)

            if (GET_PIP GREATER 0)
               message(FATAL_ERROR "Could not execute ${VENV_Python3_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/get-pip.py. Stderr content is \n${GETPIP_STDERR}\n\n"
                          "Stdout content is \n${GETPIP_STDOUT}\n")
            endif()
         endif()

         execute_process (COMMAND ${VENV_Python3_EXECUTABLE} -m pip install setuptools numpy scipy pytest
                       OUTPUT_VARIABLE PIP_INSTALL_STDOUT
                       ERROR_VARIABLE  PIP_INSTALL_STDERR
                       RESULT_VARIABLE PIP_INSTALL)

         if (PIP_INSTALL GREATER 0)
            message(FATAL_ERROR "'pip install setuptools numpy scipy pytest' failed. Stderr content is \n${PIP_INSTALL_STDERR}\n\n"
                          "Stdout content is \n${PIP_INSTALL_STDOUT}\n")
         endif()

      endif (NOT EXISTS ${VENV_DIR})

   else(VENV_TESTING)

      if (DARLING)
         set(TEST_PYTHON_EXECUTABLE ${DARLING} shell /Library/Frameworks/Python.framework/Versions/3.12/bin/python3)
      else()
         set(TEST_PYTHON_EXECUTABLE ${Python3_EXECUTABLE})
      endif()

      set(TEST_PYTHON_WRKDIR ${CMAKE_BINARY_DIR}/swig)

   endif(VENV_TESTING)

   # Look for python module wheel

   set(PY_BIN_FILES "$<TARGET_FILE:${pytarget}>" "$<TARGET_FILE:${RESHOP_LIBNAME}>")

   # libmmd.dll seems to be necessary ...
   if (WINDOWS AND CMAKE_C_COMPILER_ID MATCHES "IntelLLVM")
      cmake_path(GET CMAKE_C_COMPILER PARENT_PATH ICX_CL_DIR)

      list(APPEND PY_BIN_FILES ${ICX_CL_DIR}/libmmd.dll)
   endif()

   string(TIMESTAMP TAG_BUILD %Y%m%d%H%M UTC)

   add_custom_command(
      OUTPUT ${PYTHON_PROJECT_DIR}/dist/timestamp
      COMMAND ${CMAKE_COMMAND} -E remove_directory dist reshop
      COMMAND ${CMAKE_COMMAND} -E make_directory reshop
      COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_BINARY_DIR}/swig/reshop.py reshop
      COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_BINARY_DIR}/swig/__init__.py reshop
      COMMAND ${CMAKE_COMMAND} -E copy ${PY_BIN_FILES} reshop
      # bdist_egg
      COMMAND ${VENV_Python3_EXECUTABLE} setup.py egg_info --tag-build=dev${TAG_BUILD} bdist_wheel -p ${Python3_platform}
      COMMAND ${CMAKE_COMMAND} -E touch dist/timestamp
      MAIN_DEPENDENCY
        ${CMAKE_BINARY_DIR}/swig/setup.py.gen
      DEPENDS
        $<TARGET_FILE:${pytarget}>
        $<TARGET_FILE:${RESHOP_LIBNAME}>
        ${PYTHON_PROJECT_DIR}/setup.py
      BYPRODUCTS
        ${PYTHON_PROJECT_DIR}/ReSHOP.egg-info
        ${PYTHON_PROJECT_DIR}/build
        ${PYTHON_PROJECT_DIR}/dist
        ${PYTHON_PROJECT_DIR}/reshop
      WORKING_DIRECTORY 
        ${PYTHON_PROJECT_DIR}
      COMMAND_EXPAND_LISTS)

   # Main Target
   add_custom_target(${pytarget}_package ALL
                     DEPENDS ${PYTHON_PROJECT_DIR}/dist/timestamp
                     WORKING_DIRECTORY ${PYTHON_PROJECT_DIR})

   ###################
   ##  Python Test  ##
   ###################

   if (DISTRIB AND CMAKE_SYSTEM_NAME MATCHES "Linux")
      find_program(AUDITWHEEL auditwheel REQUIRED)

      if (NOT PYTHON_WHEEL_PLAT)
         MESSAGE(ERROR "PYTHON_WHEEL_PLAT needs to be set")
      endif()

      add_custom_command(TARGET ${pytarget}_package POST_BUILD
         COMMAND sh -c "${AUDITWHEEL} repair --plat ${PYTHON_WHEEL_PLAT} -w ${OUTPUT_DIR} dist/*.whl"
         WORKING_DIRECTORY ${PYTHON_PROJECT_DIR}
         COMMENT "Properly tag wheel"
         VERBATIM)

   else() # Other than linux (with distrib) just copy the wheel

      add_custom_command(TARGET ${pytarget}_package POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory dist ${OUTPUT_DIR}
            WORKING_DIRECTORY ${PYTHON_PROJECT_DIR}
            COMMENT "Copy wheel to output"
            VERBATIM)

   endif() #Linux
  
   if(VENV_TESTING)
     add_custom_command(TARGET ${pytarget}_package POST_BUILD
                        COMMAND ${VENV_Python3_EXECUTABLE} -m pip uninstall -qqy reshop
                        COMMAND ${VENV_Python3_EXECUTABLE} -m pip install --find-links=${PYTHON_PROJECT_DIR}/dist reshop
                        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
                        COMMENT "Installing reshop in venv"
                        VERBATIM)
   endif()

   if (DARLING)
      set(PYTEST_DIR /Users/xhub/LinuxHome/reshop/swig/tests/)
   else(DARLING)
      set(PYTEST_DIR ${CMAKE_CURRENT_SOURCE_DIR}/tests/)
   endif(DARLING)

   if (APPLE OR LINUX)
      set (PYTEST_OPTS "--rootdir=/tmp")
   endif()

   add_test(NAME python-${pytarget}-tests
            COMMAND ${TEST_PYTHON_EXECUTABLE} -m pytest -q -s ${PYTEST_OPTS} ${PYTEST_DIR}
            WORKING_DIRECTORY ${TEST_PYTHON_WRKDIR})

   set (ENVVARS)

   if (APPLE)
      list(APPEND ENVVARS "DYLD_FALLBACK_LIBRARY_PATH=${CMAKE_BINARY_DIR}/test")
   endif()

   if (VENV_TESTING)
      if (WITH_BACKWARD)
         # We rely on libbackward being copied
         list(APPEND ENVVARS "LD_LIBRARY_PATH=${CMAKE_BINARY_DIR}/test")
      endif()
   endif()

   if (RHP_PATH_FILENAME)
      list(APPEND ENVVARS "RHP_PATH_FILENAME=${RHP_PATH_FILENAME}")
      message(STATUS "[python] Setting RHP_PATH_FILENAME=${RHP_PATH_FILENAME}")
   endif()

   if (ENVVARS)
      message(STATUS "[python] Setting env vars ${ENVVARS}")
      set_tests_properties(python-${pytarget}-tests PROPERTIES ENVIRONMENT "${ENVVARS}")
   endif()

endmacro()

# Main loop
separate_arguments(BUILD_PYTHON_LIST NATIVE_COMMAND "${BUILD_PYTHON}")
foreach (P IN LISTS BUILD_PYTHON_LIST)
   SETUP_SWIG_PYTHON_MODULE(${P})
endforeach()
