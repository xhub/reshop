

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
set(LINUX_LINKER_COMMON   -Bsymbolic --build-id=uuid --gc-sections --warn-common --warn-execstack)

set(CMAKE_C_FLAGS_ASAN
  "${CMAKE_C_FLAGS_DEBUG} ${ASAN_COMPILE_FLAGS}" CACHE STRING
  "Flags used by the C compiler for Asan build type or configuration." FORCE)

set(LINKER_FLAGS)

# LINUX is only defined for cmake >= 3.25?
if (CMAKE_SYSTEM_NAME MATCHES "Linux")

   foreach(_f ${LINUX_LINKER_COMMON})
      list(APPEND LINKER_FLAGS LINKER:${_f})
   endforeach()

   foreach(_z ${LINUX_LINKER_COMMON_Z})
      list(APPEND LINKER_FLAGS LINKER:-z,${_z})
   endforeach()

   if (CMAKE_LINKER IN_LIST "mold;ld.lld")
      list(APPEND LINKER_FLAGS "LINKER:--gdb-index")
      add_compile_options(-ggnu-pubnames)
   endif()

elseif(APPLE)

endif()
