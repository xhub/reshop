

# TODO
# Consider using CMAKE_LINKER_TYPE for higher granularity

##############################################
# GNU Linker options
#
# z ones:
# - defs: report unresolved symbol references
# - noexecstack: report object does not need execstack
# - now: resolve all symbols when the program is started, or when the shared library is loaded  by  dlopen
#        the opposite would be lazy
# - origin: Specify that the object requires $ORIGIN handling in paths
# - relro: specifies a memory segment that should be made read-only after relocation, if supported
# - text:  error if dynamic relocations are created in read-only sections
#
#
# -Bsymbolic: [shared library] bind references to global symbols to the definition within the library.
#  They are then not overrideable from the program that would link against it
#
# --build-id=uuid:    Add build-id into the binary
# --error-execstack:  turn execstack warnings into errors
# --gc-sections:      GC of unused data and functions
# --warn-common:      warn about a common symbol being combined with another on or a symbol definition
# --warn-execstack:   warn about executable stack (required for --error-execstack?)
# 
#############################################

set(LINUX_LINKER_COMMON_Z defs noexecstack now origin relro)
# --error-execstack is not supported by lld
set(LINUX_LINKER_COMMON       -Bsymbolic --build-id=uuid --gc-sections --warn-common --warn-execstack)
set(LINUX_LINKER_COMMON_BFD   --warn-execstack --error-execstack)
set(LINUX_LINKER_COMMON_LLD   --gdb-index)
set(LINUX_LINKER_COMMON_MOLD  --gdb-index)

set(LINKER_VERSION_REGEXP_BFD   "^GNU ld.* ([0-9]+\\.[0-9]+)")
set(LINKER_VERSION_REGEXP_LLD   "^LLD ([0-9]+\\.[0-9]+)")
set(LINKER_VERSION_REGEXP_MOLD  "^mold ([0-9]+\\.[0-9]+)")

set(CMAKE_C_FLAGS_ASAN
  "${CMAKE_C_FLAGS_DEBUG} ${ASAN_COMPILE_FLAGS}" CACHE STRING
  "Flags used by the C compiler for Asan build type or configuration." FORCE)

set(LINKER_FLAGS)

# LINUX is only defined for cmake >= 3.25?
# Todo: check for issues with crosscompilation
if (CMAKE_SYSTEM_NAME MATCHES "Linux")

   # Check if the linker is GNU ld
   execute_process(
    COMMAND ${CMAKE_LINKER} --version
    OUTPUT_VARIABLE LINKER_VERSION_OUTPUT
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
   )

   # Check if GNU ld is part of the version output
   if (LINKER_VERSION_OUTPUT MATCHES "^GNU ld")
      set(LINKER_FLAVOR "BFD")

   elseif(LINKER_VERSION_OUTPUT MATCHES "^LLD")
      set(LINKER_FLAVOR "LLD")
   elseif(LINKER_VERSION_OUTPUT MATCHES "^mold")
      set(LINKER_FLAVOR "MOLD")
   else()
      message(WARNING "Unknown linker from output of ${CMAKE_LINKER} --version:\n ${LINKER_VERSION_OUTPUT}")
      return()
   endif()

   # Extract the version number
   string(REGEX MATCH ${LINKER_VERSION_REGEXP_${LINKER_FLAVOR}} LINKER_VERSION_MATCH ${LINKER_VERSION_OUTPUT})
   if (LINKER_VERSION_MATCH)
      set(LINKER_VERSION "${CMAKE_MATCH_1}")
   else()
      message(WARNING "Could not determine GNU ld version from: ${LINKER_VERSION_OUTPUT}")
   endif()
   message(STATUS "Linker: ${LINKER_FLAVOR} version ${LINKER_VERSION} detected")

   foreach(_f ${LINUX_LINKER_COMMON})
      list(APPEND LINKER_FLAGS LINKER:${_f})
   endforeach()

   foreach(_z ${LINUX_LINKER_COMMON_Z})
      list(APPEND LINKER_FLAGS LINKER:-z,${_z})
   endforeach()

   if (LINKER_FLAVOR IN_LIST "LLD;MOLD")
      add_compile_options(-ggnu-pubnames)
   endif()

   if ((NOT LINKER_FLAVOR STREQUAL "BFD") OR LINKER_VERSION VERSION_GREATER_EQUAL "2.39")
      foreach(_f ${LINUX_LINKER_COMMON_${LINKER_FLAVOR}})
         list(APPEND LINKER_FLAGS LINKER:${_f})
      endforeach()
   endif()

   set(CMAKE_LINKER_TYPE ${LINKER_FLAVOR})

   if (CMAKE_VERSION VERSION_LESS 3.29)
      string(TOLOWER ${LINKER_FLAVOR} COMPILER_LINKER_VALUE)
      set(CMAKE_EXE_LINKER_FLAGS    "${CMAKE_EXE_LINKER_FLAGS} -fuse-ld=${COMPILER_LINKER_VALUE}")
      set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fuse-ld=${COMPILER_LINKER_VALUE}")
   endif()

elseif(APPLE)

endif()
