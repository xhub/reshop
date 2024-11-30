
if (NOT BUILD_PYTHON)
   return()
endif()


cmake_minimum_required(VERSION 3.22)
find_package(SWIG REQUIRED)
include(UseSWIG)

# linux target for python wheel
if (LINUX AND DISTRIB)
   set(PYTHON_WHEEL_PLAT "manylinux_2_17_x86_64")
endif()


# We generate the python bindings in 2 steps:
# - 1. Use SWIG to generate the wrapper (just python for now)
# - 2. For each specified python interpreter, compile the module

# This is our hackish way to ensure proper tracking of dependencies of
# included swig ".i" files
file(GLOB SWIG_I_INCLUDES *.i)

set_property(SOURCE reshop.i PROPERTY C ON)
set_property(SOURCE reshop.i PROPERTY SWIG_MODULE_NAME reshop)
set_property(SOURCE reshop.i PROPERTY OUTPUT_DIR ${CMAKE_BINARY_DIR}/swig)
set_property(SOURCE reshop.i PROPERTY USE_SWIG_DEPENDENCIES ON)
set(CMAKE_SWIG_FLAGS -O -Wall -Werror -doxygen -python -builtin)

# Use internal macros, but that's how it is
swig_module_initialize(swig_py_reshop PYTHON)
swig_add_source_to_module(swig_py_reshop reshop_py_wrap reshop.i)

# Add Custom target to keep track of things
add_custom_target(swig_py_reshop DEPENDS ${LIBRESHOP} ${SWIG_I_INCLUDES})

# Configure the two files outside of the loop
configure_file( ${CMAKE_SOURCE_DIR}/swig/setup.py.in ${CMAKE_BINARY_DIR}/swig/setup.py.gen @ONLY)
file(GENERATE OUTPUT ${CMAKE_BINARY_DIR}/swig/__init__.py
              CONTENT "__version__ = \"${ReSHOP_VERSION}\"\nfrom .reshop import *")


macro(SETUP_SWIG_PYTHON_MODULE Py3_EXECUTABLE)

   # With GMSPython, we expect the configuration to be stored as a cmake file
   if (${Py3_EXECUTABLE} MATCHES "GMSPython") 
      if (NOT GMSPythonCfg)
         MESSAGE(ERROR "When GMSPython is in BUILD_PYTHON, then GMSPythonCfg must be specified" )
      endif()

      if (NOT Python3_EXECUTABLE)
         MESSAGE(ERROR "When GMSPython is in BUILD_PYTHON, then Python3_EXECUTABLE must be set to a compatible python
         interpreter" )
      endif()

      include(${GMSPythonCfg})
   else ()

      # Regular python discovery. We need to force the version to the one
      # specified as argument
      set(Python3_EXECUTABLE ${Py3_EXECUTABLE})
   
      execute_process (COMMAND ${Python3_EXECUTABLE} "--version"
                       OUTPUT_VARIABLE PyCmd_VERSION
                       OUTPUT_STRIP_TRAILING_WHITESPACE)
      separate_arguments (outlist UNIX_COMMAND ${PyCmd_VERSION})
   
      LIST(POP_BACK outlist Py3_VERSION)
   
      # Unset a bunch of stuff to ensure find_package does something ...
      unset(Python3_Found)
      unset(Python3_Library)
      unset(Python3_NumPy_INCLUDE_DIRS)
   
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
   set(reshop_py_sources ${reshop_py_wrap} ${CMAKE_SOURCE_DIR}/src/utils/os/asprintf.c)

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

   set_property(TARGET ${pytarget} APPEND PROPERTY LINK_OPTIONS "${Python3_LINK_OPTIONS}")
   target_link_libraries(${pytarget} ${RESHOP_LIBNAME})
   target_include_directories(${pytarget} PRIVATE ${CMAKE_SOURCE_DIR}/swig
                                                  ${Python3_INCLUDE_DIRS}
                                                  ${Python3_NumPy_INCLUDE_DIRS})

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


   # Look for python module wheel

   add_custom_command(
     OUTPUT ${PYTHON_PROJECT_DIR}/dist/timestamp
     COMMAND ${CMAKE_COMMAND} -E remove_directory dist reshop
     COMMAND ${CMAKE_COMMAND} -E make_directory reshop
     COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_BINARY_DIR}/swig/reshop.py reshop
     COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_BINARY_DIR}/swig/__init__.py reshop
     COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${pytarget}> reshop
     COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${RESHOP_LIBNAME}> reshop
     # bdist_egg
     COMMAND ${Python3_EXECUTABLE} setup.py egg_info  --tag-date --tag-build=dev bdist_wheel -p ${Python3_platform}
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
     DEPENDS
       ${PYTHON_PROJECT_DIR}/dist/timestamp
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
         COMMAND sh -c "${AUDITWHEEL} repair --plat ${PYTHON_WHEEL_PLAT} dist/*.whl"
         WORKING_DIRECTORY ${PYTHON_PROJECT_DIR}
         COMMENT "Properly tag wheel"
         VERBATIM)
   endif() #Linux
      
   if(VENV_TESTING)
     # Testing using a vitual environment
     set(VENV_EXECUTABLE ${Python3_EXECUTABLE} -m venv)
     #set(VENV_EXECUTABLE ${Python3_EXECUTABLE} -m venv)
     set(VENV_DIR ${CMAKE_CURRENT_BINARY_DIR}/${pytarget}/venv)

     if(WIN32)
       if (NEED_WINE)
         set(VENV_Python3_EXECUTABLE "wine;${VENV_DIR}/Scripts/python.exe")
       else(NEED_WINE)
         set(VENV_Python3_EXECUTABLE "${VENV_DIR}/Scripts/python.exe")
         endif(NEED_WINE)
     else()
       set(VENV_Python3_EXECUTABLE ${VENV_DIR}/bin/python)
     endif()

      
     # make a virtualenv to install our python package in it
     add_custom_command(TARGET ${pytarget}_package POST_BUILD
       # Clean previous install otherwise pip install may do nothing
       COMMAND ${CMAKE_COMMAND} -E remove_directory ${VENV_DIR}
       COMMAND ${VENV_EXECUTABLE} ${VENV_DIR}
       # Must NOT call it in a folder containing the setup.py otherwise pip call it
       # (i.e. "python setup.py bdist") while we want to consume the wheel package
       COMMAND ${VENV_Python3_EXECUTABLE} -m pip install --find-links=${PYTHON_PROJECT_DIR}/dist reshop pytest
       BYPRODUCTS ${VENV_DIR}
       WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
       COMMENT "Create venv and install reshop"
       VERBATIM)

      set(TEST_PYTHON_EXECUTABLE ${VENV_Python3_EXECUTABLE})
      # Set working directory to top-level. Avoid loading wrong module
      set(TEST_PYTHON_WRKDIR ${CMAKE_BINARY_DIR})
   else(VENV_TESTING)
      if (DARLING)
         set(TEST_PYTHON_EXECUTABLE ${DARLING} shell /Library/Frameworks/Python.framework/Versions/3.12/bin/python3)
      else()
         set(TEST_PYTHON_EXECUTABLE ${Python3_EXECUTABLE})
      endif()

      set(TEST_PYTHON_WRKDIR ${CMAKE_BINARY_DIR}/swig)
   endif()

   if (DARLING)
      set(PYTEST_DIR /Users/xhub/LinuxHome/reshop/swig/tests/)
   else(DARLING)
      set(PYTEST_DIR ${CMAKE_SOURCE_DIR}/swig/tests/)
   endif(DARLING)

   if (APPLE OR LINUX)
      set (PYTEST_OPTS "--rootdir=/tmp")
   endif()
   add_test(NAME ${pytarget}-tests
            COMMAND ${TEST_PYTHON_EXECUTABLE} -m pytest ${PYTEST_OPTS} ${PYTEST_DIR}
            WORKING_DIRECTORY ${TEST_PYTHON_WRKDIR})
   
endmacro()

# Main loops
foreach (P IN LISTS BUILD_PYTHON)
   SETUP_SWIG_PYTHON_MODULE(${P})

endforeach()
