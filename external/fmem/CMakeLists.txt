# Copyright (C) 2017 Franklin "Snaipe" Mathieu.
# Redistribution and use of this file is allowed according to the terms of the MIT license.
# For details see the LICENSE file distributed with Mimick.

cmake_minimum_required (VERSION 2.8.12)

option(BUILD_SHARED_LIBS "Build shared library." OFF)
option(BUILD_TESTING "Build tests." OFF)

if (POLICY CMP0048)
  # cmake warns if fmem is included from a parent directory whose CMakeLists.txt
  # requires a CMake version of 3.0 or later
  cmake_policy(SET CMP0048 NEW)
endif()

project (fmem C)

OPTION(BUILD_TESTING "Build the tests with ON, defaults to OFF" OFF)

list (APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/.cmake")

if(MSVC AND NOT BUILD_SHARED_LIBS)
  add_compile_options(/EHsc)
  # See https://gitlab.kitware.com/cmake/cmake/-/issues/18390
  add_compile_options(
          $<$<CONFIG:>:/MT>
          $<$<CONFIG:Debug>:/MTd>
          $<$<CONFIG:Release>:/MT>
  )
endif(MSVC AND NOT BUILD_SHARED_LIBS)

include (CheckSymbolExists)
include (CheckCSourceCompiles)
include (GNUInstallDirs)

if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
  set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99 -Wall -Wextra -pedantic")
endif ()

list (APPEND CMAKE_REQUIRED_DEFINITIONS
  -D_GNU_SOURCE
  -D_CRT_RAND_S
  -DVC_EXTRALEAN
  -DWIN32_LEAN_AND_MEAN)

check_symbol_exists (open_memstream stdio.h HAVE_OPEN_MEMSTREAM)
check_symbol_exists (fopencookie stdio.h HAVE_FOPENCOOKIE)
check_symbol_exists (funopen stdio.h HAVE_FUNOPEN)
check_symbol_exists (tmpfile stdio.h HAVE_TMPFILE)
check_symbol_exists (rand_s stdlib.h HAVE_WINAPI_RAND_S)
check_symbol_exists (CreateFile windows.h HAVE_WINAPI_CREATEFILE)
check_symbol_exists (CloseHandle windows.h HAVE_WINAPI_CLOSEHANDLE)
check_symbol_exists (GetFileSize windows.h HAVE_WINAPI_GETFILESIZE)
check_symbol_exists (CreateFileMapping windows.h HAVE_WINAPI_CREATEFILEMAPPING)
check_symbol_exists (MapViewOfFile windows.h HAVE_WINAPI_MAPVIEWOFFILE)
check_symbol_exists (UnmapViewOfFile windows.h HAVE_WINAPI_UNMAPVIEWOFFILE)
check_symbol_exists (GetTempPath windows.h HAVE_WINAPI_GETTEMPPATH)
check_symbol_exists (_open_osfhandle io.h HAVE_WINAPI_OPEN_OSFHANDLE)
check_symbol_exists (_get_osfhandle io.h HAVE_WINAPI_GET_OSFHANDLE)
check_symbol_exists (_fdopen stdio.h HAVE_WINAPI_FDOPEN)
check_symbol_exists (_fileno stdio.h HAVE_WINAPI_FILENO)
check_symbol_exists (_close io.h HAVE_WINAPI_CLOSE)

set (SOURCES)

if (HAVE_OPEN_MEMSTREAM)
  list (APPEND SOURCES src/fmem-open_memstream.c)
elseif (HAVE_FOPENCOOKIE)
  list (APPEND SOURCES
    src/alloc.c
    src/alloc.h
    src/fmem-fopencookie.c)
elseif (HAVE_FUNOPEN)
  list (APPEND SOURCES
    src/alloc.c
    src/alloc.h
    src/fmem-funopen.c)
elseif (HAVE_WINAPI_CREATEFILE
    AND HAVE_WINAPI_CLOSEHANDLE
    AND HAVE_WINAPI_GETFILESIZE
    AND HAVE_WINAPI_CREATEFILEMAPPING
    AND HAVE_WINAPI_MAPVIEWOFFILE
    AND HAVE_WINAPI_UNMAPVIEWOFFILE
    AND HAVE_WINAPI_GETTEMPPATH
    AND HAVE_WINAPI_FDOPEN
    AND HAVE_WINAPI_FILENO
    AND HAVE_WINAPI_CLOSE
    AND HAVE_WINAPI_OPEN_OSFHANDLE
    AND HAVE_WINAPI_GET_OSFHANDLE
    AND HAVE_WINAPI_RAND_S)
  list (APPEND SOURCES src/fmem-winapi-tmpfile.c)
elseif (HAVE_TMPFILE)
  list (APPEND SOURCES src/fmem-tmpfile.c)
else ()
  message (FATAL_ERROR "No memory stream implementation found")
endif ()

add_library (fmem ${SOURCES})
target_include_directories (fmem PUBLIC ${PROJECT_BINARY_DIR}/gen)

get_property (FMEM_LIBTYPE
  TARGET fmem
  PROPERTY TYPE)

set (CMAKE_REQUIRED_DEFINITIONS -fvisibility=hidden)
check_c_source_compiles (
  "__attribute__((visibility(\"default\"))) int main(void) { return 0; }"
  CC_HAVE_VISIBILITY)
set (CMAKE_REQUIRED_DEFINITIONS)

if ("${FMEM_LIBTYPE}" MATCHES "SHARED_LIBRARY")
  if (WIN32)
    set (EXPORT_MACROS
"#ifdef FMEM_BUILD_LIBRARY
# define FMEM_API __declspec(dllexport)
#else /* !FMEM_BUILD_LIBRARY */
# define FMEM_API __declspec(dllimport)
#endif /* !FMEM_BUILD_LIBRARY */")
    add_definitions (-DFMEM_BUILD_LIBRARY)
  elseif (CC_HAVE_VISIBILITY)
    set (EXPORT_MACROS "#define FMEM_API __attribute__((visibility(\"default\")))")
    set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fvisibility=hidden")
  endif ()
else ()
  set (EXPORT_MACROS "#define FMEM_API")
endif ()

configure_file (
  ${PROJECT_SOURCE_DIR}/include/fmem.h.in
  ${PROJECT_BINARY_DIR}/gen/fmem.h
  @ONLY)

install(TARGETS fmem
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})

install(FILES
  ${PROJECT_BINARY_DIR}/gen/fmem.h
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

include (CTest)

if (BUILD_TESTING)
  find_package (Criterion REQUIRED)
  add_subdirectory (test)
endif ()
