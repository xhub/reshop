# WE use
# 3.21 file(COPY_FILE ..)
# 3.20: add_compile_definitions 
# Bump to 3.25 for LINUX, and kill compat code below
cmake_minimum_required (VERSION 3.21)

# This needs to be set at the top
# We need that for TLS
set(CMAKE_OSX_DEPLOYMENT_TARGET 10.9)

# Set cmake modules directory (i.e. the one which contains all user-defined FindXXX.cmake files among other things)
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

include (CMakeDependentOption)

project (ReSHOP LANGUAGES C
                VERSION          "0.1.8"
                HOMEPAGE_URL     "https://reshop.eu/")

#custom stuff
include(compiler_flags)

# Keep that on top
set (CMAKE_C_STANDARD 17)
set (CMAKE_C_STANDARD_REQUIRED ON)
set (CMAKE_C_EXTENSIONS OFF)
set (CMAKE_C_VISIBILITY_PRESET hidden)

if (CMAKE_SYSTEM_NAME MATCHES "Windows")
   set(WINDOWS 1)
   # Poor man's detection of not using MSVC libc
   # Can't use MSVC as the compiler may try not to emulate cl.exe
   if (NOT CYGWIN AND NOT MINGW)
      add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
      add_compile_definitions(_CRT_NONSTDC_NO_WARNINGS)
   endif()
else()
   set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif()


if (CMAKE_VERSION VERSION_LESS 3.25)
   if (CMAKE_SYSTEM_NAME MATCHES "Linux")
      set(LINUX 1)
   endif()
endif()


# Use C++14 (for PPL and backward)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set (CMAKE_CXX_VISIBILITY_PRESET hidden)

if (LINUX)
   SET(CMAKE_INSTALL_RPATH "\$ORIGIN")
   SET(CMAKE_BUILD_RPATH_USE_ORIGIN TRUE)
   SET(CMAKE_BUILD_RPATH "\$ORIGIN")
endif ()

#################################################
# Valgrind (keep that on top)
#################################################
find_program( MEMORYCHECK_COMMAND valgrind )
# Use --gen-suppressions=all to help create proper suppression file
#set(MEMORYCHECK_COMMAND_OPTIONS "--trace-children=yes --leak-check=full --track-origins=yes --gen-suppressions=all" )
set(MEMORYCHECK_COMMAND_OPTIONS "--trace-children=yes --leak-check=full --track-origins=yes" )
set(MEMORYCHECK_SUPPRESSIONS_FILE "${CMAKE_SOURCE_DIR}/cmake/valgrind.supp")

# This has to come AFTER setting the valgrind options and before adding the
# test subdirectory
include(CTest)

include(sanitizers-profile)

# Start options
option(BUILD_GAMS_DRIVER 
  "Build the GAMS glue to use ReSHOP within the GAMS system" OFF)

option(GAMSSYSDIR 
   "GAMS system directory (as string)" "")

option(DISTRIB
  "Build for the distribution of files: on Linux, disables recent glibc symbols" OFF)

option(BUILD_DOCUMENTATION "Create and install the HTML based API documentation (requires Doxygen)" OFF)

option(BUILD_PYTHON
   "Build a python bindings (argument is a list of python interpreter)" "")

option(DEV_MODE
   "Developper mode (enables extras)" OFF)

CMAKE_DEPENDENT_OPTION (WITH_BACKTRACE 
  "Build with backtracing facilities" ON
  "NOT WINDOWS;NOT DISTRIB;NOT DEV_MODE" OFF)

CMAKE_DEPENDENT_OPTION (WITH_BACKWARD
  "Build with the backward backtracing library" ON
  "NOT WINDOWS;NOT APPLE;NOT DISTRIB;NOT DEV_MODE" OFF)

#################################################
# Less common options
#################################################

option(FORCE_BACKWARD
  "Advanced: Force the use of the backward libs" OFF)

option(DISABLE_COMPILER_OPT
  "Advanced: Disable compilation optimization" OFF)

option(WITH_VALGRIND
   "Advanced: Use valgrind marking on custom memory allocation" OFF)

option(GCC_ANALYZER
  "Advanced: Enable use of the GCC analyzer" OFF)

option(BUILD_INSTALLER
   "Advanced: Build a makeself installer" OFF)

option(INSTALL_COMMON_MAKESELF
   "Advanced: Install the library files in a common makeself target" OFF)

option(EXTRA_DEBUG_OUTPUT
   "Advanced: Compile additional output statements for debug" OFF)

#################################################
# TODO: cleanup the following
#################################################

option(WITH_HDF5 
  "Support HDF5 export" OFF)

option(WITH_SOLVERS
  "Add the code to build solvers" OFF)

option(WITH_FMEM
   "Compile with FMEM" OFF)


#################################################
# Start of the configuration
#################################################

if (FORCE_BACKWARD)
   set(WITH_BACKWARD ON)
   set(WITH_BACKTRACE ON)
endif()


# Setup version / git hash
set (pre_configure_dir "${CMAKE_SOURCE_DIR}/src/conf")
include (CheckGit)
CheckGitSetup()
# If we are not in a git repo, use CMAKE_PROJECT_VERSION
if (NOT EXISTS ${CMAKE_BINARY_DIR}/git-state.txt)
   set(GIT_HASH ${CMAKE_PROJECT_VERSION})
   configure_file(${pre_configure_dir}/git_version.c.in ${CMAKE_BINARY_DIR}/generated/git_version.c @ONLY)
endif()


# Some sanity
# TODO: use add_link_options
if (NOT WINDOWS AND NOT APPLE)
   set(CMAKE_LINK_WHAT_YOU_USE TRUE)
endif ()

# Maybe disable that for CC (esp. windows)
include(CheckIPOSupported)
check_ipo_supported(RESULT HAS_IPO)

# In dev mode, we want to have more output
if(DEV_MODE)
   add_compile_definitions (RHP_OUTPUT_BACKTRACE)
   add_compile_definitions (RHP_DEBUGGER_STOP)
endif()

if(EXTRA_DEBUG_OUTPUT)
   add_compile_definitions (RHP_EXTRA_DEBUG_OUTPUT)
endif()



#################################################
# Platform-specific part
#################################################

if (LINUX) # GLIBC-specific for now
   SET(_GNU_SOURCE 1)
   if (DISTRIB)
      add_compile_definitions (COMPAT_GLIBC)
   endif(DISTRIB)
endif(LINUX)

if (APPLE)
   if (OSXCROSS_HOST)
      # This seems needed for the rpath madness -- 2024.01.23
      set(CMAKE_SHARED_LIBRARY_RUNTIME_C_FLAG "-Wl,-rpath,")

      set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-rpath,@loader_path")
      set(CMAKE_EXE_MODULE_FLAGS "${CMAKE_EXE_MODULE_FLAGS} -Wl,-rpath,@loader_path")
   endif()

   SET(_DARWIN_C_SOURCE 1)
   # Tell cmake to use "@rpath" as part of the install_name dir (INSTALL_NAME_DIR)
   SET(CMAKE_MACOSX_RPATH TRUE)
   # The above does not seem to be enough
   SET(CMAKE_INSTALL_NAME_DIR "@rpath")
   # Tell CMAKE to use INSTALL_NAME_DIR already at build time
   SET(CMAKE_BUILD_WITH_INSTALL_NAME_DIR TRUE)

   # TODO: is the CMAKE_BUILD_RPATH needed?
   set(CMAKE_BUILD_RPATH "@loader_path")
   set(CMAKE_INSTALL_RPATH "@loader_path")

endif(APPLE)


#################################################
# Define reshop library
#################################################

set (RESHOP_LIBNAME "reshop")

# include directories
include_directories ("${PROJECT_BINARY_DIR}")
include_directories ("external")
include_directories ("external/itostr")
include_directories ("external/gamslink")

#This is for reshop_config.h and others
include_directories ("${CMAKE_BINARY_DIR}/generated/")

include_directories (src)
file (GLOB_RECURSE _FLIST LIST_DIRECTORIES TRUE "src/*")
foreach (_F ${_FLIST})
   if (IS_DIRECTORY ${_F})
      include_directories("${_F}")
   endif()
endforeach()

# TODO: why is this needed?
if (CMAKE_C_COMPILER_TARGET)
   FILE(WRITE "${CMAKE_BINARY_DIR}/target" "${CMAKE_C_COMPILER_TARGET}")
elseif(C_COMPILER_GNU_LIKE)
   execute_process (
    COMMAND     ${CMAKE_C_COMPILER} -dumpmachine
    OUTPUT_FILE "${CMAKE_BINARY_DIR}/target"
   )
endif()


# Add files
FILE(GLOB_RECURSE RESHOP_C_FILES src/*.c)
LIST(APPEND RESHOP_C_FILES "external/open_lib.c")
LIST(APPEND RESHOP_C_FILES "external/itostr/itostr.c")
FILE(GLOB_RECURSE GEN_C_FILES ${CMAKE_BINARY_DIR}/generated/*.c)
LIST(APPEND RESHOP_C_FILES ${GEN_C_FILES})


SET(EXTRA_LIBS ${CMAKE_DL_LIBS})

if (APPLE OR UNIX)
   LIST(APPEND EXTRA_LIBS "m")
endif (APPLE OR UNIX)


#################################################
# Process options that change library content
#################################################

if (WITH_BACKTRACE)
   add_compile_definitions(WITH_BACKTRACE)
   if (C_COMPILER_GNU_LIKE)
      add_compile_options("-fno-omit-frame-pointer")
      add_compile_options("-ggdb")
   endif()

   if (WITH_BACKWARD)
      LIST(APPEND EXTRA_LIBS "backward")
      add_subdirectory (external/backward)
      include_directories (external/backward)
      add_compile_definitions(WITH_BACKWARD)
   else (WITH_BACKWARD)
      find_package (Unwind REQUIRED)
      include_directories("${LIBUNWIND_INCLUDE_DIR}")
      LIST(APPEND EXTRA_LIBS "${LIBUNWIND_LIBRARIES}")
   endif(WITH_BACKWARD)

endif (WITH_BACKTRACE)


if (WITH_VALGRIND)
   add_compile_definitions("WITH_VALGRIND")
endif (WITH_VALGRIND)


if (WITH_FMEM)
   MESSAGE(FATAL_ERROR "FMEM support is no longer working, disable this option")
   set(BUILD_TESTING_BCK ${BUILD_TESTING})
   set(BUILD_SHARED_LIBS_BCK ${BUILD_SHARED_LIBS})
   set (BUILD_TESTING OFF)
   set(BUILD_SHARED_LIBS OFF)
   add_subdirectory (external/fmem)
   include_directories (${CMAKE_BINARY_DIR}/external/fmem/gen)
   get_property (FMEM_NAME TARGET fmem PROPERTY NAME)
   LIST(APPEND EXTRA_LIBS ${FMEM_NAME})
   #  add_compile_definitions(RHP_WITH_FMEM)
   set(BUILD_TESTING ${BUILD_TESTING_BCK})
   set(BUILD_SHARED_LIBS ${BUILD_SHARED_LIBS_BCK})
endif (WITH_FMEM)

if (WITH_HDF5)
   find_package(HDF5 REQUIRED COMPONENTS C)
   if(NOT HDF5_FOUND)
      MESSAGE(FATAL_ERROR "HDF5 requested, but no library found")
   endif(NOT HDF5_FOUND)
   add_compile_definitions("WITH_HDF5")
   include_directories("${HDF5_INCLUDE_DIRS}")
   LIST(APPEND EXTRA_LIBS "${HDF5_LIBRARIES}")
   add_compile_definitions("${HDF5_DEFINITIONS}")
endif (WITH_HDF5)

#################################################
# Management of GAMS C API files
#################################################

find_program(GAMS_EXE_HOST gams)

if(NOT GAMSSYSDIR AND GAMS_EXE_HOST)

   set (SCRDIR "${CMAKE_BINARY_DIR}/gams")
   file(REMOVE_RECURSE "${SCRDIR}")
   file(MAKE_DIRECTORY "${SCRDIR}")

   # Get GAMS sys on HOST first
   execute_process (COMMAND ${GAMS_EXE_HOST} "${CMAKE_SOURCE_DIR}/cmake/gams_sysdir.gms" "lo=3" "scrdir=${SCRDIR}"
                    RESULT_VARIABLE GAMS_RC
                    ERROR_VARIABLE GAMS_STDERR)

   if (GAMS_RC GREATER 0)
   message(FATAL_ERROR "Could not execute GAMS. Stderr content is ${GAMS_STDERR}."
                       "Use the GAMSSYSDIR to specify the GAMS system directory")
   endif()

   file(READ ${SCRDIR}/gams_data GAMS_OUTPUT)
   # TODO this should not be executed if the gams command fails
   if (GAMS_OUTPUT)
      string(REGEX MATCH "SYSDIR=([^\n]*)\n" _DUMMY ${GAMS_OUTPUT})
      if (CMAKE_MATCH_1)
         set(GAMSDIR "${CMAKE_MATCH_1}" CACHE PATH "GAMS system directory")
      else ()
         string(REGEX MATCH "SysDir=([^\n]*)\n" _DUMMY ${GAMS_OUTPUT})
         if (CMAKE_MATCH_1)
            set(GAMSDIR "${CMAKE_MATCH_1}" CACHE PATH "GAMS system directory")
         endif()
      endif()
      string(REGEX MATCH "GAMS_RELEASE=([0-9]+)\..*\n" _DUMMY ${GAMS_OUTPUT})
      if (CMAKE_MATCH_1)
         set(GAMS_VERSION "${CMAKE_MATCH_1}")
      else ()
         MESSAGE(FATAL_ERROR "INVALID \"GAMS_RELEASE\" STRING from ${CMAKE_SOURCE_DIR}/cmake/gams_sysdir.gms: ${GAMS_OUTPUT}")
      endif()
   endif()

   FILE(WRITE "${CMAKE_BINARY_DIR}/gams_version" "${GAMS_VERSION}")

   # Now we get GAMSDIR_TARGET for the tests
   # This is different from the GAMSDIR_HOST if we are crosscompiling
   if (NEED_WINE)
      set(GAMS_EXE_TARGET "wine;gams.exe")
   else(NEED_WINE)
      set(GAMS_EXE_TARGET "gams")
   endif(NEED_WINE)

   execute_process (COMMAND ${GAMS_EXE_TARGET} "${CMAKE_SOURCE_DIR}/cmake/gams_sysdir.gms" "lo=3" "scrdir=${SCRDIR}"
                    RESULT_VARIABLE GAMS_RC
                    ERROR_VARIABLE GAMS_STDERR)

   if (GAMS_RC GREATER 0)
   message(FATAL_ERROR "Could not execute target GAMS executable ${GAMS_EXE_TARGET}."
                       "Stderr content is ${GAMS_STDERR}."
                       "Use the GAMSSYSDIR to specify the GAMS system directory")
   endif()

   file(READ ${SCRDIR}/gams_data GAMS_OUTPUT)
   # TODO this should not be executed if the gams command fails
   if (GAMS_OUTPUT)
      string(REGEX MATCH "SYSDIR=([^\n]*)\n" _DUMMY ${GAMS_OUTPUT})
      if (CMAKE_MATCH_1)
         set(GAMSDIR_TARGET "${CMAKE_MATCH_1}")
      else ()
         string(REGEX MATCH "SysDir=([^\n]*)\n" _DUMMY ${GAMS_OUTPUT})
         if (CMAKE_MATCH_1)
            set(GAMSDIR_TARGET "${CMAKE_MATCH_1}")
         endif()
      endif()
   endif()

   if (GAMSDIR_TARGET)
      set(GAMS_IS_INSTALLED 1)
   endif()

   if (NEED_WINE)
      string(REGEX REPLACE "\\\\" "\\\\\\\\" GAMSDIR_TARGET ${GAMSDIR_TARGET})
   endif(NEED_WINE)

   # Generate a GAMS control file for the TARGET
   execute_process (COMMAND ${GAMS_EXE_TARGET} "${CMAKE_SOURCE_DIR}/cmake/gams_gen_gamscntr.gms" "lo=4" "scrdir=${SCRDIR}")
   SET(GAMSCNTR_DAT_FILE "${SCRDIR}/gamscntr.dat")

   MESSAGE(STATUS "GAMS SYSDIR (host): ${GAMSDIR}")
   MESSAGE(STATUS "GAMS SYSDIR (target): ${GAMSDIR_TARGET}")
   MESSAGE(STATUS "GAMS CNTR file (target): ${GAMSCNTR_DAT_FILE}")
endif()

# TODO: document this overwrite?
if (GAMSSYSDIR)
   SET(GAMSDIR "${GAMSSYSDIR}" CACHE PATH "GAMS system directory")
endif(GAMSSYSDIR)

if(NOT GAMSDIR)
   SET (GAMSDIR "${CMAKE_SOURCE_DIR}/external/gamsapi/")
endif (NOT GAMSDIR)



#################################################
# END of GAMS driver management
#################################################

# We indirectly include all files in apifiles/C/api by first having the
# headers in src/gams/wrapped included them.
include_directories ("${GAMSDIR}")

# The GAMS_C_FILES have already been included in RESHOP_C_FILES above
file(GLOB_RECURSE GAMS_C_FILES src/gams/wrapped/*.c)

if (C_COMPILER_GNU_LIKE)
   set_source_files_properties(${GAMS_C_FILES} PROPERTIES COMPILE_OPTIONS
    "-Wno-error=shadow;-Wno-format-overflow;-Wno-shadow;-Wno-pedantic;-Wno-error=pedantic")
endif()

# See https://www.gams.com/latest/docs/RN_33.html
# This is the old behaviour, otherwise we need gcmt.c
# TODO: GAMS review. Is this correct?
set_source_files_properties(${GAMS_C_FILES} PROPERTIES COMPILE_DEFINITIONS GC_NO_MUTEX)


# Add the GCC fanalyzer here
if (GCC_ANALYZER)
   set(FILES_FOR_ANALYZER ${RESHOP_C_FILES})
   list(REMOVE_ITEM FILES_FOR_ANALYZER ${GAMS_C_FILES})
   set_source_files_properties(${FILES_FOR_ANALYZER} PROPERTIES COMPILE_FLAGS -fanalyzer)
endif (GCC_ANALYZER)


# Define library as target
if (NOT SANITIZE_ADDRESS OR NOT SANITIZE_MEMORY OR NOT SANITIZE_THREAD OR NOT
    SANITIZE_UNDEFINED)
   set (RESHOP_LIBNAME_OBJ "${RESHOP_LIBNAME}_obj")
   add_library (${RESHOP_LIBNAME_OBJ} OBJECT ${RESHOP_C_FILES})
   add_library (${RESHOP_LIBNAME} SHARED $<TARGET_OBJECTS:${RESHOP_LIBNAME_OBJ}>)
   SET_C_WARNINGS(${RESHOP_LIBNAME_OBJ})
   # This looks like a hack, but otherwise we have quite an inconsistent
   # behavior
   target_compile_definitions(${RESHOP_LIBNAME_OBJ} PRIVATE ${RESHOP_LIBNAME}_EXPORTS)
   # Put functions and data in separate sections to enable GC of unused parts
   # TODO: test this
   if (C_COMPILER_GNU_LIKE)
      target_compile_options(${RESHOP_LIBNAME_OBJ} PRIVATE -ffunction-sections -fdata-sections)
   endif()

   SET(RESHOP_INTERNAL_TESTS ON)
else ()
   add_library (${RESHOP_LIBNAME} SHARED ${RESHOP_C_FILES})
   SET_C_WARNINGS(${RESHOP_LIBNAME})
endif ()


set (LIBNAMES "${RESHOP_LIBNAME}")

function(cpy4test libname)
   add_custom_command(TARGET ${libname} POST_BUILD
       # Clean previous install otherwise pip install may do nothing
       COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${libname}> ${CMAKE_BINARY_DIR}/swig
       COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${libname}> ${CMAKE_BINARY_DIR}/test
       WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
       COMMENT "Copy ${libname} library"
       VERBATIM)
endfunction()

# Always copy reshop
cpy4test("${RESHOP_LIBNAME}")

if (GAMS_IS_INSTALLED AND BUILD_TESTING)
   if (LINUX)
      set(PATHLIBS_GLOB "libpath*.so;liblusol*.so")
   elseif(APPLE)
      set(PATHLIBS_GLOB "libpath*.dylib;liblusol*.dylib")
   elseif(WINDOWS)
      set(PATHLIBS_GLOB "path*.dll;lusol*.dll")
   else()
      message(ERROR "Unsupported case")
   endif()

   # Copy all files
   foreach (_G in ${PATHLIBS_GLOB})
      file (GLOB PATHFILES ${GAMSDIR}/${_G})
      foreach (_F ${PATHFILES})
         add_custom_command(TARGET ${RESHOP_LIBNAME} POST_BUILD
             COMMAND ${CMAKE_COMMAND} -E copy ${_F} ${CMAKE_BINARY_DIR}/test
             WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
             COMMENT "Copy ${_F} for running tests"
             VERBATIM)
      endforeach()
   endforeach()
endif()

#################################################
# BUILD GAMS driver if required
#################################################

if(BUILD_GAMS_DRIVER)
   add_subdirectory(gams_link)
endif()

#################################################
# SWIG-generated wrappers
#################################################

add_subdirectory (swig)

#################
# Add linker flags #
#################


foreach(_LIBNAME ${LIBNAMES})
   SET_C_WARNINGS(${_LIBNAME})

   target_link_libraries(${_LIBNAME} PRIVATE ${EXTRA_LIBS} ${${_LIBNAME}_LIBS})


   if (CMAKE_SYSTEM_NAME MATCHES "Linux" AND (NOT CMAKE_BUILD_TYPE IN_LIST SANITIZER_PROFILES))
      target_link_options (${_LIBNAME} PRIVATE
      LINKER:--warn-common LINKER:--allow-shlib-undefined
      LINKER:--no-undefined LINKER:-z,defs LINKER:-z,now LINKER:-z,relro)
      # GC of unused data and functions
      # Need to test this
      target_link_options (${_LIBNAME} PRIVATE LINKER:--gc-sections)
      # uncomment for debugging
      # target_link_options (${_LIBNAME} PRIVATE LINKER:--print-gc-sections)
   else (CMAKE_SYSTEM_NAME MATCHES "Windows")
      set_target_properties(${_LIBNAME} PROPERTIES IMPORT_SUFFIX ".lib")
   endif()

   if(HAS_IPO)
      set_target_properties(${_LIBNAME} PROPERTIES INTERPROCEDURAL_OPTIMIZATION TRUE)
   endif()

endforeach()

add_subdirectory (test)

add_subdirectory (benchmark)


# Install part
if (BUILD_GAMS_DRIVER)
   if (BUILD_INSTALLER)
      INSTALL(TARGETS ${LIBNAMES} DESTINATION ${CMAKE_BINARY_DIR}/makeself)
   else()
      INSTALL(TARGETS ${LIBNAMES} DESTINATION ${GAMSDIR})
   endif()

   if (INSTALL_COMMON_MAKESELF)
      INSTALL(TARGETS ${LIBNAMES} DESTINATION ${CMAKE_BINARY_DIR}/makeself)
   endif()
endif ()

# This is a way to get the version in a file in order to use it while building
# makeself and it is also useful in some Makefile target.
configure_file(${CMAKE_SOURCE_DIR}/cmake/version.in ${CMAKE_BINARY_DIR}/version @ONLY)
configure_file(${CMAKE_SOURCE_DIR}/cmake/reshop_config.h.in ${CMAKE_BINARY_DIR}/generated/reshop_config.h)

# TODO? delete?
if (WITH_CPPCHECK)
   find_program(CMAKE_C_CPPCHECK NAMES cppcheck)
   find_program(CMAKE_CXX_CPPCHECK NAMES cppcheck)
   if (CMAKE_CXX_CPPCHECK)
      list(
          APPEND CMAKE_C_CPPCHECK 
              "--enable=all"
              "--inconclusive"
              "--force" 
              "--quiet"
              "--xml"
              "--inline-suppr"
              "--suppressions-list=${CMAKE_SOURCE_DIR}/cmake/CppCheckSuppressions.txt"
      )
      list(
          APPEND CMAKE_CXX_CPPCHECK 
              "--enable=all"
              "--inconclusive"
              "--force" 
              "--quiet"
              "--xml"
              "--inline-suppr"
              "--suppressions-list=${CMAKE_SOURCE_DIR}/cmake/CppCheckSuppressions.txt"
      )
   endif()
endif()


add_subdirectory (docs)
